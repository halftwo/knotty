#include "FcgiClient.h"
#include "fastcgi.h"
#include "dlog/dlog.h"
#include "xslib/XEvent.h"
#include "xslib/xnet.h"
#include "xslib/loc.h"
#include "xslib/iobuf.h"
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <memory>

#define CONNECT_TIMEOUT		(2*1000)
#define SHUTDOWN_TIMEOUT	(5*1000)
#define CONNECT_INTERVAL	(1*1000)
#define RETRY_INTERVAL		(15*1000)
#define REAP_INTERVAL		(300*1000)

#define CON_MAX			1024


class FcgiConnection: public XEvent::FdHandler, public XEvent::TaskHandler, private XMutex
{
	FcgiClientPtr _client;
	FcgiQuestPtr _quest;
	FcgiAnswerPtr _answer;

	int _fd;
	bool _shutdown;
	bool _autoretry;
	int _timeout;
	int _reqmax;
	int _reqseq;

	enum {
		ST_CONNECT,
		ST_IDLE,
		ST_WRITE,
		ST_READ,
		ST_CLOSED,
	} _state;

	bool _reading_content;
	bool _found_newline;
	int _errno;

        loc_t _iloc;
        iobuf_t _ib;
        unsigned char _ibuf[1024];

	FCGI_Header _header;
	xstr_t _body;
	unsigned char _padding[256];
        ssize_t _ipos;
	ssize_t _ibytes;

        loc_t _oloc;
        struct iovec *_ov;
        int _ov_num;

	int _dispose_fcgi_body();
	void _operate(const FcgiQuestPtr& quest);

	int do_read(const XEvent::DispatcherPtr& dispatcher);
	int do_write(const XEvent::DispatcherPtr& dispatcher);
	void do_close(const XEvent::DispatcherPtr& dispatcher);
	int on_shutdown_timeout();

public:
	FcgiConnection(FcgiClient* client, int timeout, int reqmax, bool autoretry);
	virtual ~FcgiConnection();

	void operate(const FcgiQuestPtr& quest);
	void shutdown();
	bool ok() const				{ return (!_shutdown && _state == ST_IDLE); }
	void connect();
	int error_no() const			{ return _errno; }

	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);
	virtual void event_on_task(const XEvent::DispatcherPtr& dispatcher);
};


FcgiConnection::FcgiConnection(FcgiClient* client, int timeout, int reqmax, bool autoretry)
	: _client(client)
{
	_timeout = (timeout > 0) ? timeout : INT_MAX;
	_reqmax = (reqmax > 0) ? reqmax: INT_MAX;
	_autoretry = autoretry;
	_reqseq = 0;
	_fd = -1;
	_shutdown = false;
	_reading_content = false;
	_found_newline = false;
	_ibytes = 0;
	_errno = 0;
}

FcgiConnection::~FcgiConnection()
{
	if (_fd >= 0)
		::close(_fd);
	xlog(XLOG_VERBOSE, "~FcgiConnection(): reqseq=%d", _reqseq);
}

void FcgiConnection::_operate(const FcgiQuestPtr& quest)
{
	XEvent::DispatcherPtr dispatcher = _client->dispatcher();
	if (_state >= ST_CLOSED) 
	{
		_client->releaseClosed(this, quest);
		return;
	}

	assert(_state == ST_IDLE);
	assert(!_quest);
	_quest = quest;
	_answer.reset(FcgiAnswer::create(_quest->request_uri()));
	_state = ST_WRITE;

	if (do_write(dispatcher) < 0)
		do_close(dispatcher);
	else
		dispatcher->replaceTask(this, _timeout);
}

void FcgiConnection::operate(const FcgiQuestPtr& quest)
{
	Lock lock(*this);
	_operate(quest);
}

void FcgiConnection::shutdown()
{
	Lock lock(*this);
	_shutdown = true;
	_client->dispatcher()->addTask(XEvent::TaskHandler::create(this, &FcgiConnection::on_shutdown_timeout), SHUTDOWN_TIMEOUT);
}

int FcgiConnection::on_shutdown_timeout()
{
	Lock lock(*this);
	do_close(_client->dispatcher());
	return 0;
}

void FcgiConnection::connect()
{
	if (_fd >= 0)
	{
		::close(_fd);
		_fd = -1;
	}

	_reading_content = false;
	_found_newline = false;
	_state = ST_CONNECT;
	LOC_RESET(&_iloc);
	LOC_RESET(&_oloc);

	std::string location = _client->server();
	if (location.find_first_of('/', 0) != std::string::npos)
	{
		_fd = xnet_unix_connect_nonblock(location.c_str());
	}
	else
	{
		size_t pos = location.find_last_not_of("0123456789", std::string::npos);
		if (pos >= location.length() || location[pos] != '+')
		{
			throw XERROR_FMT(XError, "invalid net location (%s), should be in \"host+port\" format", location.c_str());
		}

		std::string host = location.substr(0, pos);
		int port = atoi(location.substr(pos + 1).c_str());

		_fd = xnet_tcp_connect_nonblock(host.c_str(), port);
	}

	if (_fd < 0)
	{
		throw XERROR_FMT(XError, "xnet_tcp_connect_nonblock() failed, location=%s, errno=%d", location.c_str(), errno);
	}

	xnet_set_tcp_nodelay(_fd);
	xnet_set_keepalive(_fd);
	xnet_set_linger_on(_fd, 0);
	_ib = make_iobuf(_fd, _ibuf, sizeof(_ibuf));
	_client->dispatcher()->addFd(this, _fd, XEvent::READ_EVENT | XEvent::WRITE_EVENT | XEvent::EDGE_TRIGGER);
	_client->dispatcher()->addTask(this, CONNECT_TIMEOUT);
}

void FcgiConnection::event_on_task(const XEvent::DispatcherPtr& dispatcher)
{
	Lock lock(*this);
	if (_quest)
	{
		// Timeout quest will not be retried
		_quest->finish(XERROR_MSG(XError, "Timeout on requesting the Fcgi server"));
		_quest.reset();
	}

	if (_fd >= 0)
	{
		dlog("FCGI_ERROR", "server=%s, state(%d) timeout", _client->server().c_str(), _state);
		do_close(dispatcher);
	}
}

void FcgiConnection::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	bool should_close = false;

	Lock lock(*this);
	if (events & XEvent::READ_EVENT)
	{
		if (do_read(dispatcher) < 0)
			should_close = true;
	}

	if (events & XEvent::CLOSE_EVENT)
	{
		should_close = true;
	}

	if (!should_close && (events & XEvent::WRITE_EVENT))
	{
		// Don't call do_write() if socket is closing.
		if (do_write(dispatcher) < 0)
			should_close = true;
	}

	if (should_close)
	{
		do_close(dispatcher);
	}
}

void FcgiConnection::do_close(const XEvent::DispatcherPtr& dispatcher)
{
	if (_fd >= 0)
	{
		dispatcher->removeFd(this);
		dispatcher->removeTask(this);
		::close(_fd);
		_fd = -1;
		_ib.cookie = (void *)-1;
		_state = ST_CLOSED;
		_client->releaseClosed(this, _quest);
		_quest.reset();
	}
}

/* 0 continue, 1 success, -1 error
 */
int FcgiConnection::_dispose_fcgi_body()
{
	if (_header.type == FCGI_STDOUT)
	{
		static const xstr_t STATUS_XS = XSTR_CONST("Status");
		if (_reading_content)
		{
			_answer->append_content(_body.data, _body.len);
		}
		else
		{
			xstr_t xs = _body;
			xstr_t line;
			while (xs.len && xstr_delimit_char(&xs, '\n', &line))
			{
				if (_found_newline && (line.len == 0 || (line.len == 1 && line.data[0] == '\r')))
				{
					_reading_content = true;
					break;
				}

				_found_newline = (xs.data != NULL);

				xstr_t key, value;
				xstr_key_value(&line, ':', &key, &value);
				if (xstr_equal(&key, &STATUS_XS))
				{
					_answer->set_status(xstr_atoi(&value));
				}
				else if (xstr_equal(&key, &XIC4FCGI_VERSION_XS))
				{
					_answer->set_xic4fcgi(xstr_atoi(&value));
				}
			}
			ssize_t header_len = _reading_content ? xs.data - _body.data : _body.len;
			_answer->append_header(_body.data, header_len);
			if (header_len < _body.len)
			{
				_answer->append_content(_body.data + header_len, _body.len - header_len);
			}
		}
	}
	else if (_header.type == FCGI_STDERR)
	{
		static const xstr_t STDERR_XS = XSTR_CONST("STDERR");
		zdlog(NULL, &STDERR_XS, &_quest->request_uri(), &_body);
		_answer->append_stderr(_body.data, _body.len);
	}
	else if (_header.type == FCGI_END_REQUEST)
	{
		if (_body.len != sizeof(FCGI_EndRequestBody))
		{
			dlog("FCGI_ERROR", "FCGI_EndRequestBody len invalid, should be %zd instead of %zd",
				sizeof(FCGI_EndRequestBody), _body.len);
		}
		else
		{
			FCGI_EndRequestBody* endreq = (FCGI_EndRequestBody*)_body.data;
			if (endreq->protocolStatus != FCGI_REQUEST_COMPLETE)
			{
				throw XERROR_MSG(XError, "Fcgi not completed");
			}
		}
		ostk_free(_answer->ostk(), _body.data);
		return 1;
	}
	else
	{
		dlog("FCGI_ERROR", "unknonw fcgi response type %d, len=%zd", _header.type, _body.len);
		ostk_free(_answer->ostk(), _body.data);
		return -1;
	}

	return 0;
}

int FcgiConnection::do_read(const XEvent::DispatcherPtr& dispatcher)
{
        LOC_BEGIN(&_iloc);

	while (true)
	{
		_ipos = 0;
		LOC_ANCHOR
		{
			ssize_t rc = iobuf_read(&_ib, (char *)&_header + _ipos, sizeof(FCGI_Header) - _ipos);
			if (rc < 0)
			{
				_errno = errno;
				xlog(XLOG_DEBUG, "iobuf_read()=%zd q=%p reqseq=%d fd=%d errno=%d",
						rc, _quest.get(), _reqseq, _fd, errno);
				goto error;
			}

			_ibytes += rc;
			_ipos += rc;
			if (_ipos < (ssize_t)sizeof(FCGI_Header))
			{
				LOC_PAUSE(0);
			}

			_body.len = ((size_t)_header.contentLengthB1 << 8) + _header.contentLengthB0;
			_body.data = OSTK_ALLOC(_answer->ostk(), unsigned char, _body.len);
		}

		_ipos = 0;
		LOC_ANCHOR
		{
			ssize_t rc = iobuf_read(&_ib, _body.data + _ipos, _body.len - _ipos);
			if (rc < 0)
			{
				_errno = errno;
				xlog(XLOG_DEBUG, "iobuf_read()=%zd q=%p reqseq=%d fd=%d errno=%d",
						rc, _quest.get(), _reqseq, _fd, errno);
				goto error;
			}

			_ibytes += rc;
			_ipos += rc;
			if (_ipos < _body.len) 
			{
				LOC_PAUSE(0);
			}
		}

		if (_header.paddingLength > 0)
		{
			_ipos = 0;	
			LOC_ANCHOR
			{
				ssize_t rc = iobuf_read(&_ib, _padding + _ipos, _header.paddingLength - _ipos);
				if (rc < 0)
				{
					_errno = errno;
					xlog(XLOG_DEBUG, "iobuf_read()=%zd q=%p reqseq=%d fd=%d errno=%d",
							rc, _quest.get(), _reqseq, _fd, errno);
					goto error;
				}

				_ibytes += rc;
				_ipos += rc;
				if (_ipos < _header.paddingLength) 
				{
					LOC_PAUSE(0);
				}
			}
		}

		int rc = _dispose_fcgi_body();
		if (rc > 0)
		{
			break;
		}
		else if (rc < 0)
		{
			goto error;
		}
	}

	if (!_quest)
	{
		dlog("FCGI_ERROR", "No request waiting for answer");
		goto error;
	}

	if (_ib.len != 0)
	{
		dlog("FCGI_FATAL", "More data pending for reading. This may be caused by my own bug or fcgi server bug");
		goto error;
	}

	LOC_RESET(&_iloc);

	_quest->finish(_answer);
	_quest.reset();
	_answer.reset();
	_ibytes = 0;
	_reading_content = false;
	_found_newline = false;
	_state = ST_IDLE;
	++_reqseq;

	if (_reqseq >= _reqmax)
	{
		xlog(XLOG_DEBUG, "reqseq=%d reqmax=%d fd=%d", _reqseq, _reqmax, _fd);
		goto error;
	}

	{
		FcgiQuestPtr q = _client->getQuestOrPutback(this);
		if (q)
		{
			try {
				_operate(q);
			}
			catch (std::exception& ex)
			{
				goto error;
			}
		}
	}

	return 1;

error:
	LOC_END(&_iloc);
	if (_quest)
	{
		if (_state != ST_READ && _ibytes == 0 && !_quest->isRetry() && _autoretry)
		{
			// Only when we do not finish the writing and 
			// read nothing, we can retry it safely. 
			// We retry at most once.
			xlog(XLOG_NOTICE, "Retry quest %p", _quest.get());
			_quest->setRetry();
		}
		else
		{
			xlog(XLOG_INFO, "Read %zd bytes from fcgi server", _ibytes);
			XERROR_VAR_FMT(XError, ex, "%s response from fcgi server, errno=%d", (_ibytes > 0) ? "Incomplete" : "No", errno);
			_quest->finish(ex);
			_quest.reset();
		}
	}
	return -1;
}

int FcgiConnection::do_write(const XEvent::DispatcherPtr& dispatcher)
{
	if (_state == ST_CONNECT)
	{
		_state = ST_IDLE;
		_quest = _client->getQuestOrPutback(this);
		if (_quest)
		{
			_state = ST_WRITE;
			_answer.reset(FcgiAnswer::create(_quest->request_uri()));
			dispatcher->replaceTask(this, _timeout);
		}
	}

	LOC_BEGIN(&_oloc);

	LOC_ANCHOR
	{
		if (!_quest || _state != ST_WRITE)
			LOC_PAUSE(0);
	}

	_ov = _quest->get_iovec(&_ov_num);

	LOC_ANCHOR
	{
		ssize_t rc = xnet_writev_nonblock(_fd, _ov, _ov_num);
		if (rc < 0)
		{
			_errno = errno;
			if (rc == -1)
				xlog(XLOG_INFO, "server=%s, xnet_writev_nonblock()=%zd, reqseq=%d, fd=%d, errno=%d", _client->server().c_str(), rc, _reqseq, _fd, errno); 
			goto error;
		}

		_ov_num = xnet_adjust_iovec(&_ov, _ov_num, rc);
		if (_ov_num > 0)
		{
			LOC_PAUSE(0);
		}
		_ov = NULL;

	}
	_state = ST_READ;
	_ibytes = 0;
	LOC_RESET(&_oloc);
	return 1;

error:
	LOC_END(&_oloc);
	return -1;
}


FcgiClient::FcgiClient(const XEvent::DispatcherPtr& dispatcher, const FcgiConfig& config)
	: _dispatcher(dispatcher), _conf(config)
{
	_conf.conmax = XS_CLAMP(_conf.conmax, 1, CON_MAX);
	_shutdown = false;	
	_idle = 0;
	_idlestack.reserve(_conf.conmax);
	_err_count = 0;
	_con_count = 0;
}

FcgiClient::~FcgiClient()
{
	if (!_queue.empty())
	{
		XERROR_VAR_MSG(XError, ex, "FcgiClient destructed");
		do {
			FcgiQuestPtr quest = _queue.front();
			_queue.pop_front();
			quest->finish(ex);
		} while (!_queue.empty());
	}
}

void FcgiClient::_new_connection()
{
	// lock is acquired
	FcgiConnectionPtr c(new FcgiConnection(this, _conf.timeout, _conf.reqmax, _conf.autoretry));
	_cons.insert(c);
	try {
		c->connect(); 
	}
	catch (std::exception& ex)
	{
		releaseClosed(c.get(), FcgiQuestPtr());
	}
}

void FcgiClient::process(const FcgiQuestPtr& quest)
{
	FcgiConnectionPtr con;
	{
		Lock lock(*this);
		if (_shutdown)
		{
			quest->finish(XERROR_MSG(XError, "FcgiClient shutdown"));
			return;
		}

		while (!_idlestack.empty())
		{
			FcgiConnectionPtr c = _idlestack.back();
			_idlestack.pop_back();
			if (c->ok())
			{
				con = c;
				break;
			}
		}

		if (_idle > (int)_idlestack.size())
			_idle = _idlestack.size();

		if (!con)
		{
			_queue.push_back(quest);

			if (_cons.size() < (size_t)_conf.conmax)
			{
				_new_connection();
			}
		}
	}

	if (con)
	{
		con->operate(quest);
	}
}

void FcgiClient::start()
{
	_dispatcher->addTask(XEvent::TaskHandler::create(this, &FcgiClient::on_reap_timer), REAP_INTERVAL);
}

void FcgiClient::shutdown()
{
	std::set<FcgiConnectionPtr> cons;
	{
		Lock lock(*this);
		_shutdown = true;
		_idlestack.clear();
		_cons.swap(cons);
	}

	for (std::set<FcgiConnectionPtr>::iterator iter = cons.begin(); iter != cons.end(); ++iter)
	{
		(*iter)->shutdown();
	}
}

int FcgiClient::on_reap_timer()
{
	FcgiConnectionPtr con;
	{
		Lock lock(*this);
		if (_idle && _cons.size() > 1)
		{
			std::set<FcgiConnectionPtr>::iterator iter = _cons.begin();
			con = *iter;
			_cons.erase(iter);
		}
		_idle = _idlestack.size();
	}

	if (con)
	{
		con->shutdown();
	}

	return _shutdown ? 0 : REAP_INTERVAL;
}

void FcgiClient::releaseClosed(FcgiConnection* con, const FcgiQuestPtr& quest)
{
	std::deque<FcgiQuestPtr> queue;
	int err = con->error_no();

	{
		Lock lock(*this);

		_cons.erase(FcgiConnectionPtr(con));
		if (quest)
		{
			_queue.push_front(quest);
		}

		++_err_count;
		if (_err_count > _con_count)
		{
			/* Only when _err_count > _con_count, we can be sure
			   that the quests have been tried at least one more
			   time and still not ok. 
			 */
			xlog(XLOG_INFO, "err_count=%d con_count=%d queue.size=%zd", _err_count, _con_count, _queue.size());
			_err_count = 0;
			_con_count = 0;
			_queue.swap(queue);
		}

		if (_cons.size() < (size_t)_conf.conmax && !_queue.empty())
		{
			int num = _conf.conmax - _cons.size();
			if (num > (int)_queue.size())
				num = _queue.size();
			for (int i = 0; i < num; ++i)
			{
				_new_connection();
			}
		}
	}

	if (!queue.empty())
	{
		XERROR_VAR_FMT(XError, ex, "Connections to the Fcgi server are broken, errno=%d", err);
		do {
			FcgiQuestPtr quest = queue.front();
			queue.pop_front();
			quest->finish(ex);
		} while (!queue.empty());
	}
}

FcgiQuestPtr FcgiClient::getQuestOrPutback(FcgiConnection* con)
{
	FcgiQuestPtr quest;

	Lock lock(*this);
	_err_count = 0;
	_con_count = _cons.size();

	if (_queue.size())
	{
		quest = _queue.front();
		_queue.pop_front();
	}
	else if (!_shutdown)
	{
		_idlestack.push_back(FcgiConnectionPtr(con));
		_dispatcher->removeTask(con);
	}

	return quest;
}

