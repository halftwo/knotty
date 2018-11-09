#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif
#include "PtEngineImp.h"
#include "VData.h"
#include "XicException.h"
#include "XicMessage.h"
#include "dlog/dlog.h"
#include "xslib/Enforce.h"
#include "xslib/cxxstr.h"
#include "xslib/uuid.h"
#include "xslib/xbase32.h"
#include "xslib/vbs.h"
#include "xslib/xlog.h"
#include "xslib/xnet.h"
#include "xslib/hashint.h"
#include "xslib/rdtsc.h"
#include "xslib/ScopeGuard.h"
#include "xslib/unix_user.h"
#include "xslib/XThread.h"
#include "xslib/msec.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <utility>
#include <sstream>

#define DEFAULT_STACK_SIZE	(1024*1024)
#define CON_REAP_INTERVAL	(17*1000)	// msec
#define SHUTDOWN_WAIT		15		// seconds

#define PT_ENGINE_VERSION	"PT." XIC_VERSION

static char engine_version_rcsid[] = "$xic: " PT_ENGINE_VERSION " $";

using namespace xic;


XMutex PtResult::_mutex;
XCond PtResult::_conds[PtResult::COND_MASK + 1];


bool PtResult::isSent() const
{
	XLock<XMutex> lock(_mutex);
	return _isSent;
}

void PtResult::waitForSent()
{
	int x = hash32_uintptr((uintptr_t)this) & COND_MASK;

	XLock<XMutex> lock(_mutex);
	_waitSent = true;
	while (!_isSent)
	{
		_conds[x].wait(lock);
	}
}

bool PtResult::isCompleted() const
{
	XLock<XMutex> lock(_mutex);
	return (_answer || _ex.get());
}

void PtResult::waitForCompleted()
{
	int x = hash32_uintptr((uintptr_t)this) & COND_MASK;

	XLock<XMutex> lock(_mutex);
	_waitCompleted = true;
	while (!_answer && !_ex.get())
		_conds[x].wait(lock);
}

AnswerPtr PtResult::takeAnswer(bool throw_ex)
{
	waitForCompleted();

	UniquePtr<XError> ex;
	AnswerPtr r;

	{
		XLock<XMutex> lock(_mutex);
		if (_ex.get())
			std::swap(_ex, ex);
		else
			_answer.swap(r);
	}

	if (ex.get())
	{
		if (throw_ex)
		{
			ex->do_throw();
		}

		return except2answer(*ex, _quest->method(), _quest->service(), _con ? _con->endpoint() : "", true);
	}
	else if (r)
	{
		if (throw_ex && r->status() != 0)
		{
			VDict vd(r->args_dict());

			xstr_t name = vd.getXstr("exname");
			long code = vd.getInt("code");
			xstr_t tag = vd.getXstr("tag");
			xstr_t message = vd.getXstr("message");
			xstr_t raiser = vd.getXstr("raiser");
			const vbs_dict_t *detail = vd.get_dict("detail");
			throw RemoteException(__FILE__, __LINE__, code, make_string(tag), make_string(message),
					make_string(name), make_string(raiser), detail);
		}
	}

	return r;
}

bool PtResult::retry()
{
	{
		XLock<XMutex> lock(_mutex);
		if (_retryNumber > 0)
			return false;

		++_retryNumber;
		_isSent = false;
	}
	return _prx->retryQuest(this);
}

void PtResult::giveError(const XError& ex)
{
	assert(!_answer && !_ex.get());

	UniquePtr<XError> e(ex.clone());

	if (_con && isBad(_con->state()))
	{
		_prx->onConnectionError(_con, _quest);
	}

	if (xic_dlog_cae)
	{
		int64_t txid = _quest->txid();
		const xstr_t& service = _quest->service();
		const xstr_t& method = _quest->method();
		xdlog(vbs_xfmt, NULL, "XIC.CEL", NULL,
			"%jd Q=%.*s::%.*s E=%s",
			(intmax_t)txid, XSTR_P(&service), XSTR_P(&method), 
			ex.what());
	}

	if (_completion)
	{
		{
			XLock<XMutex> lock(_mutex);
			assert(!_ex.get() && !_answer);
			std::swap(_ex, e);
		}

		try
		{
			_completion->completed(ResultPtr(this));
		}
		catch (std::exception& ex)
		{
			XError* x = dynamic_cast<XError*>(&ex);
			dlog("WARNING", "EXCEPTION from completion callback: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
		}
	}
	else
	{
		int x = hash32_uintptr((uintptr_t)this) & COND_MASK;
		XLock<XMutex> lock(_mutex);
		assert(!_ex.get() && !_answer);
		std::swap(_ex, e);
		_conds[x].broadcast();
	}
}

void PtResult::giveAnswer(AnswerPtr& answer)
{
	assert(!_answer && !_ex.get());

	if (_completion)
	{
		{
			XLock<XMutex> lock(_mutex);
			assert(!_ex.get() && !_answer);
			_answer.swap(answer);
		}

		try
		{
			_completion->completed(ResultPtr(this));
		}
		catch (std::exception& ex)
		{
			XError* x = dynamic_cast<XError*>(&ex);
			dlog("WARNING", "EXCEPTION from completion callback: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
		}
	}
	else
	{
		int x = hash32_uintptr((uintptr_t)this) & COND_MASK;
		XLock<XMutex> lock(_mutex);
		assert(!_ex.get() && !_answer);
		_answer.swap(answer);
		if (_waitCompleted)
		{
			_conds[x].broadcast();
		}
	}
}

void PtResult::questSent()
{
	assert(!_isSent);

	_isSent = true;
	if (_completion)
	{
		try
		{
			_completion->sent(ResultPtr(this));
		}
		catch (std::exception& ex)
		{
			XError* x = dynamic_cast<XError*>(&ex);
			dlog("WARNING", "EXCEPTION from completion callback: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
		}
	}
	else
	{
		int x = hash32_uintptr((uintptr_t)this) & COND_MASK;
		XLock<XMutex> lock(_mutex);
		if (_waitSent)
		{
			_conds[x].broadcast();
		}
	}
}

void PtConnection::init()
{
	LOC_RESET(&_iloc);
	_ib = make_iobuf(_fd, _ibuf, sizeof(_ibuf));

	LOC_RESET(&_oloc);
	_ov = NULL;
	_ov_num = 0;
	_recent_active = true;
	_active_time = 0;

	if (_msg_timeout <= 0 && xic_timeout_message > 0)
		_msg_timeout = xic_timeout_message;

	_writeTimeoutTask = XTimerTask::create(this, &PtConnection::on_write_timeout);
}

PtConnection::PtConnection(PtEngine* engine, const std::string& service, const std::string& endpoint, int attempt)
	: EventHandler(true), ConnectionI(false, attempt, 0, 0, 0), _engine(engine)
{
	xref_inc();

	xic::EndpointInfo ei;
	parseEndpoint(make_xstr(endpoint), ei);
	_msg_timeout = ei.timeout;
	_close_timeout = ei.close_timeout;
	_connect_timeout = ei.connect_timeout;
	_service = service;
	_proto = make_string(ei.proto);
	_host = make_string(ei.host);
	_endpoint = ei.str();

	char buf[128];
	if (ei.host.len)
		xstr_copy_cstr(&ei.host, buf, sizeof(buf));
	else
		strcpy(buf, "127.0.0.1");

	/* XXX: If host is a domain instead of IP, we may be blocked */
	xnet_inet_sockaddr_t addr;
	if (xnet_ip46_sockaddr(buf, ei.port, (struct sockaddr *)&addr) < 0)
		throw XERROR_FMT(XError, "xnet_ip46_sockaddr() failed, host=%s", buf);

	_fd = xnet_tcp_connect_sockaddr_nonblock((struct sockaddr *)&addr, sizeof(addr));
	if (_fd < 0)
		throw XERROR_FMT(XSyscallError, "xnet_tcp_connect_sockaddr_nonblock() failed, addr=%s+%d errno=%d %m", buf, ei.port, errno);

	xnet_set_tcp_nodelay(_fd);
	xnet_set_keepalive(_fd);

	_sock_port = xnet_get_sock_ip_port(_fd, _sock_ip);
	if (addr.family == AF_INET6)
		xnet_ipv6_ntoa(addr.a6.sin6_addr.s6_addr, _peer_ip);
	else
		xnet_ipv4_ntoa(ntohl(addr.a4.sin_addr.s_addr), _peer_ip);
        _peer_port = ei.port;

	char info[128];
	int len = snprintf(info, sizeof(info), "%s/%s+%d/%s+%d", _proto.c_str(), _sock_ip, _sock_port, _peer_ip, _peer_port);
	_info = std::string(info, len);

	this->init();
	xref_dec_only();
}

PtConnection::PtConnection(PtEngine* engine, PtAdapter* adapter, int fd, int timeout, int close_timeout)
	: EventHandler(true), ConnectionI(true, 0, timeout, close_timeout, 0),
	_engine(engine), _fd(fd)
{
	xref_inc();
	_adapter.reset(adapter);
	_sock_port = xnet_get_sock_ip_port(fd, _sock_ip);
	_peer_port = xnet_get_peer_ip_port(fd, _peer_ip);

	_proto = "tcp";

	std::stringstream ss;
	ostream_printf(ss, "%s+%s+%d", _proto.c_str(), _sock_ip, _sock_port);
	if (timeout > 0 || close_timeout)
	{
		ostream_printf(ss, " timeout=%d", timeout);
		if (close_timeout)
			ostream_printf(ss, ",%d", close_timeout);
	}
	_endpoint = ss.str();

	if (xic_dlog_debug)
		dlog("XIC.DEBUG", "peer=%s+%d #=connection accepted", _peer_ip, _peer_port);

	char info[128];
	int len = snprintf(info, sizeof(info), "%s/%s+%d/%s+%d", _proto.c_str(), _sock_ip, _sock_port, _peer_ip, _peer_port);
	_info = std::string(info, len);

	this->init();
	_state = ST_WAITING_HELLO;
	xref_dec_only();
}

PtConnection::~PtConnection()
{
	iobuf_finish(&_ib);
	disconnect();
}

void PtConnection::set_exception(XError *ex)
{
	_state = ST_ERROR;
	_ex_time = _engine->time();
	_ex.reset(ex);
}

void PtConnection::start()
{
	if (_incoming)
	{
		_engine->getServerDispatcher()->addFd(this, _fd, XEvent::READ_EVENT | XEvent::WRITE_EVENT | XEvent::EDGE_TRIGGER);

		_shadowBox = _engine->getShadowBox();
		if (_shadowBox && !_shadowBox->empty())
		{
			CheckWriter cw("AUTHENTICATE");
			cw.param("method", "SRP6a");
			Lock lock(*this);
			_ck_state = CK_S1;
			send_kmsg(cw.take());
			_engine->getTimer()->replaceTask(this, connectTimeout());
		}
		else
		{
			XicMessagePtr msg = HelloMessage::create();
			_ck_state = CK_FINISH;
			Lock lock(*this);
			_state = ST_ACTIVE;
			_send_qmsg(lock, msg);
		}
	}
	else
	{
		_engine->getClientDispatcher()->addFd(this, _fd, XEvent::READ_EVENT | XEvent::WRITE_EVENT | XEvent::EDGE_TRIGGER);
		_engine->getTimer()->addTask(_writeTimeoutTask, connectTimeout());
	}
}

void PtConnection::_grace(Lock& lock)
{
	if (_state < ST_CLOSE)
	{
		_state = ST_CLOSE;
		if (_processing == 0 && _resultMap.size() == 0)
		{
			_state = ST_CLOSING;
			_graceful = true;
			try {
				_send_qmsg(lock, ByeMessage::create());
			}
			catch (std::exception& ex)
			{
				if (xic_dlog_warn)
					dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());
			}
			_engine->getTimer()->replaceTaskLaterThan(this, closeTimeout());
		}
	}
}

bool PtConnection::reap_idle(time_t now, time_t before)
{
	Lock lock(*this);
	if (_processing || _resultMap.size() || _recent_active)
	{
		_active_time = now;
		_recent_active = false;
		return false;
	}

	if (_state >= ST_CLOSE)
		return true;

	if (_active_time < before)
	{
		_grace(lock);
		return true;
	}

	return false;
}

void PtConnection::close(bool force)
{
	if (force)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d #=closing connection forcefully", _peer_ip, _peer_port);

		disconnect();
	}
	else
	{
		Lock lock(*this);
		_grace(lock);
	}
	// TODO
}

ProxyPtr PtConnection::createProxy(const std::string& service)
{
	if (service.find('@') != std::string::npos)
		throw XERROR_MSG(ServiceParseException, service);

	return _engine->_makeFixedProxy(service, this);
}

void PtConnection::setAdapter(const AdapterPtr& adapter)
{
	Lock lock(*this);
	_adapter = adapter;
}

AdapterPtr PtConnection::getAdapter() const
{
	Lock lock(*this);
	return _adapter;
}

int PtConnection::disconnect()
{
	_engine->getTimer()->removeTask(this);
	if (_writeTimeoutTask)
		_engine->getTimer()->removeTask(_writeTimeoutTask);

	if (_incoming)
	{
		_engine->getServerDispatcher()->removeFd(this);
	}
	else
	{
		_engine->getClientDispatcher()->removeFd(this);
	}

	MessageQueue wq;
	std::map<int64_t, ResultIPtr> theMap;
	UniquePtr<XError> ex;

	{
		Lock lock(*this);
		if (_state < ST_CLOSED)
			_state = ST_CLOSED;
		if (_fd >= 0)
		{
			::close(_fd);
			_fd = -1;
			_adapter.reset();
		}

		_writeTimeoutTask.reset();
		_wq.clear();
		_resultMap.take(theMap);
		if (theMap.size() && _ex.get())
		{
			ex.reset(_ex->clone());
		}
	}

	if (theMap.size())
	{
		if (!ex.get())
			ex.reset(new XERROR_MSG(ConnectionClosedException, _info));

		for (std::map<int64_t, ResultIPtr>::iterator iter = theMap.begin(); iter != theMap.end(); ++iter)
		{
			ResultI* res = iter->second.get();
			bool retried = false;
			if (_graceful || !res->isSent())
				retried = res->retry();

			if (!retried)
				res->giveError(*ex.get());
		}
	}

	_engine->check_stop();
	return 0;
}


int PtConnection::recv_msg(XicMessagePtr& msg)
{
	Lock lock(*this);

	if (_state >= ST_CLOSED)
		return 0;

	try
	{
		uint32_t bodySize = -1;
		char *p;

		iobuf_bstate_clear(&_ib);
		LOC_BEGIN(&_iloc);
		while (true)
		{
			LOC_ANCHOR
			{
				ssize_t rc = iobuf_peek(&_ib, sizeof(_iHeader), &p);
				if (rc < 0)
				{
					if (_state < ST_WAITING_HELLO)
						throw XERROR_MSG(ConnectFailedException, _info);

					if (rc == -2)
					{
						if (_state >= ST_CLOSING)
							return -1;
						if (_state >= ST_ACTIVE)
							throw XERROR_MSG(ConnectionLostException, _info);
						else if (_ck_state > CK_INIT)
							throw XERROR_MSG(AuthenticationException, _info);
						else
							throw XERROR_MSG(ConnectFailedException, _info);
					}

					throw XERROR_FMT(SocketException, "%s errno=%d", _info.c_str(), errno);
				}

				_recent_active = true;
				if (rc < (ssize_t)sizeof(_iHeader))
					LOC_PAUSE(0);
			}
			memcpy(&_iHeader, p, sizeof(_iHeader));
			iobuf_skip(&_ib, sizeof(_iHeader));

			if (_iHeader.magic != 'X')
				throw XERROR_FMT(ProtocolException, "%s Invalid packet header magic %#x", _info.c_str(), _iHeader.magic);

			if (_iHeader.version != '!')
				throw XERROR_FMT(ProtocolException, "%s Unknown packet version %#x", _info.c_str(), _iHeader.version);

			if (_iHeader.flags & ~XIC_FLAG_MASK)
				throw XERROR_FMT(ProtocolException, "%s Unknown packet header flag %#x", _info.c_str(), _iHeader.flags);

			_flagCipher = (_iHeader.flags & (XIC_FLAG_CIPHER_MODE0 | XIC_FLAG_CIPHER_MODE1));
			if (_flagCipher && !_cipher)
			{
				throw XERROR_FMT(ProtocolException, "%s CIPHER flag set but No CIPHER negociated before", _info.c_str());
			}

			if (_flagCipher == (XIC_FLAG_CIPHER_MODE0 | XIC_FLAG_CIPHER_MODE1))
			{
				throw XERROR_FMT(ProtocolException, "%s CIPHER flag mode0 and mode1 both set", _info.c_str());
			}

			bodySize = xnet_m32(_iHeader.bodySize);
			if (bodySize > xic_message_size)
				throw XERROR_FMT(MessageSizeException, "%s Huge packet bodySize %lu>%u",
					_info.c_str(), (unsigned long)bodySize, xic_message_size);

			if (_iHeader.msgType == 'H')
			{
				if (bodySize != 0)
					throw XERROR_FMT(ProtocolException, "%s Invalid hello message", _info.c_str());

				if (_state < ST_ACTIVE)
				{
					_state = ST_ACTIVE;
					if (_resultMap.size() == 0)
						_engine->getTimer()->removeTask(this);
				
					if (_write_q(lock) < 0)
						goto done;
				}
				continue;
			}
			else if (_iHeader.msgType == 'B')
			{
				if (bodySize != 0)
					throw XERROR_FMT(ProtocolException, "%s Invalid close message", _info.c_str());

				if (xic_dlog_debug)
					dlog("XIC.DEBUG", "peer=%s+%d #=Close message received", _peer_ip, _peer_port);

				_state = ST_CLOSED;
				_graceful = true;
				goto done;
			}

			if (_flagCipher & XIC_FLAG_CIPHER_MODE0)
			{
				if (bodySize <= _cipher->extraSizeMode0())
					throw XERROR_FMT(MessageSizeException, "%s Invalid packet bodySize %lu", 
							_info.c_str(), (unsigned long)bodySize);

				LOC_ANCHOR
				{
					ssize_t rc = iobuf_peek(&_ib, sizeof(_cipher->iIV), &p);
					if (rc < 0)
					{
						if (rc == -2)
						{
							if (_state >= ST_CLOSING)
								return -1;
							throw XERROR_MSG(ConnectionLostException, _info);
						}
						throw XERROR_FMT(SocketException, "%s errno=%d", _info.c_str(), errno);
					}

					_recent_active = true;
					if (rc < (ssize_t)sizeof(_cipher->iIV))
						LOC_PAUSE(0);
				}

				memcpy(_cipher->iIV, p, sizeof(_cipher->iIV));
				iobuf_skip(&_ib, sizeof(_cipher->iIV));

				_cipher->iSeqIncreaseMode0();
				if (!_cipher->decryptCheckSequenceMode0())
					throw XERROR_FMT(ProtocolException, "%s Unmatched sequence number", _info.c_str());
			}

			bodySize = xnet_m32(_iHeader.bodySize);
			if (_flagCipher)
			{
				if (_flagCipher & XIC_FLAG_CIPHER_MODE0)
					bodySize -= _cipher->extraSizeMode0();
				else
					bodySize -= _cipher->extraSize();
			}
			_rMsg = XicMessage::create(_iHeader.msgType, bodySize);
			_ipos = 0;

			LOC_ANCHOR
			{
				xstr_t body = _rMsg->body();
				ssize_t rc = iobuf_read(&_ib, body.data + _ipos, body.len - _ipos);
				if (rc < 0)
				{
					if (rc == -2)
					{
						if (_state >= ST_CLOSING)
							return -1;
						throw XERROR_MSG(ConnectionLostException, _info);
					}
					throw XERROR_FMT(SocketException, "%s errno=%d", _info.c_str(), errno);
				}

				_ipos += rc;
				_recent_active = true;
				if (_ipos < (int)body.len)
					LOC_PAUSE(0);
			}

			if (_flagCipher)
			{
				LOC_ANCHOR
				{
					ssize_t rc = iobuf_peek(&_ib, sizeof(_cipher->iMAC), &p);
					if (rc < 0)
					{
						if (rc == -2)
						{
							if (_state >= ST_CLOSING)
								return -1;
							throw XERROR_MSG(ConnectionLostException, _info);
						}
						throw XERROR_FMT(SocketException, "%s errno=%d", _info.c_str(), errno);
					}

					_recent_active = true;
					if (rc < (ssize_t)sizeof(_cipher->iMAC))
						LOC_PAUSE(0);
				}

				memcpy(_cipher->iMAC, p, sizeof(_cipher->iMAC));
				iobuf_skip(&_ib, sizeof(_cipher->iMAC));

				xstr_t body = _rMsg->body();
				if (_flagCipher & XIC_FLAG_CIPHER_MODE0)
					_cipher->decryptStartMode0(&_iHeader, sizeof(_iHeader));
				else
					_cipher->decryptStart(&_iHeader, sizeof(_iHeader));

				_cipher->decryptUpdate(body.data, body.data, body.len);
				bool ok = _cipher->decryptFinish();
				if (!ok)
					throw XERROR_FMT(ProtocolException, "Msg body failed to decrypt, %s", _info.c_str());
			}

			if (_rMsg->msgType() == 'C')
			{
				if (_state != ST_WAITING_HELLO)
					throw XERROR_MSG(ProtocolException, "Unexpected Check message");

				_rMsg->unpack_body();
				CheckPtr check(static_cast<Check*>(_rMsg.get()));
				_rMsg.reset();
				try {
					handle_check(check);
				}
				catch (XError& ex)
				{
					if (!_incoming)
						throw;

					set_exception(ex.clone());
					_engine->getTimer()->addTask(XTimerTask::create(this, &PtConnection::disconnect), 1000);
				}
				continue;
			}

			_rMsg.swap(msg);
			_rMsg.reset();
			LOC_YIELD(1);
		}
	done:
		LOC_END(&_iloc);
		return -1;
	}
	catch (XError& ex)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());

		set_exception(ex.clone());
	}
	catch (std::exception& ex)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());

		set_exception(new XERROR_MSG(UnknownException, ex.what()));
	}

	LOC_HALT(&_iloc);
	return -1;
}

SecretBoxPtr PtConnection::getSecretBox()
{
	return _engine->getSecretBox();
}

void PtConnection::checkFinished()
{
	Lock lock(*this);
	_engine->getTimer()->removeTask(this);

	if (_state < ST_ACTIVE)
		_state = ST_ACTIVE;

	if (!_wq.empty() && _write_q(lock) < 0)
		throw *_ex;
}

void PtConnection::send_kmsg(const XicMessagePtr& msg)
{
	Lock lock(*this);
	ENFORCE(_state < ST_ACTIVE);
	_kq.push_back(msg);
	if (_write_q(lock) < 0)
		throw *_ex;
}

void PtConnection::_send_qmsg(Lock& lock, const XicMessagePtr& msg)
{
	if (msg->bodySize() > xic_message_size)
	{
		throw XERROR_FMT(MessageSizeException, "Huge packet bodySize %lu>%u",
			(unsigned long)msg->bodySize(), xic_message_size);
	}

	_recent_active = true;
	if (_state < ST_CLOSED)
	{
		bool waiting = (_state < ST_ACTIVE) ? true : _wq.size();
		_wq.push_back(msg);
		if (!waiting)
		{
			if (_write_q(lock) < 0)
				throw *_ex;
		}
	}
	else if (xic_dlog_warn)
	{
		dlog("XIC.WARN", "peer=%s+%d #=send msg (type:%d) to closed connection",
			_peer_ip, _peer_port, msg->msgType());
	}
}

void PtConnection::replyAnswer(const AnswerPtr& answer)
{
	Lock lock(*this);
	_send_qmsg(lock, answer);
	--_processing;
	if (_state == ST_CLOSE && _processing == 0 && _resultMap.size() == 0)
	{
		_state = ST_CLOSING;
		_graceful = true;
		_send_qmsg(lock, ByeMessage::create());
		_engine->getTimer()->replaceTaskLaterThan(this, closeTimeout());
	}
}

void PtConnection::sendQuest(const QuestPtr& quest, const ResultIPtr& r)
{
	quest->reset_iovec();
	bool twoway = bool(r);
	try
	{
		if (twoway)
		{
			assert(quest == r->quest());
			r->setConnection(this);

			Lock lock(*this);
			if (_state >= ST_CLOSED)
			{
				throw XERROR_MSG(ConnectionClosedException, _info);
			}

			size_t outstanding = _resultMap.size();
			int64_t txid = _resultMap.addResult(r);
			quest->setTxid(txid);
			if (_msg_timeout > 0 && outstanding == 0)
			{
				_engine->getTimer()->replaceTaskLaterThan(this, _msg_timeout);
			}
			_send_qmsg(lock, quest);
		}
		else
		{
			Lock lock(*this);
			_send_qmsg(lock, quest);
		}

		bool log_sample = !twoway && (xic_sample_client > 0 && (xic_sample_client == 1 
				|| (random() / (RAND_MAX + 1.0) * xic_sample_client) < 1));

		// NB: This should be after quest->setTxid() has been called.
		if (xic_dlog_cq || log_sample)
		{
			char locus[64];
			char *p = locus;
			if (log_sample)
				p += cli_sample_locus(locus);
			*p++ = '/';
			*p++ = r->isAsync() ? 'A' : 'S';
			*p++ = '/';
			if (_cipher)
				*p++ = '*';
			*p++ = 0;

			Quest* q = quest.get();
			const xstr_t& service = q->service();
			const xstr_t& method = q->method();
			xdlog(vbs_xfmt, NULL, "XIC.CQ", locus,
				"%u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_RAW<} %p{>VBS_RAW<}",
				_sock_port, _peer_ip, _peer_port,
				(intmax_t)q->txid(), XSTR_P(&service), XSTR_P(&method), 
				&q->context_xstr(), &q->args_xstr());
		}
	}
	catch (XError& ex)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());

		if (r)
		{
			ResultIPtr res;
			int64_t txid = quest->txid();
			if (txid > 0)	// It has been added into the _resultMap
			{
				Lock lock(*this);
				res = _resultMap.removeResult(txid);
			}
			else
			{
				res = r;
			}

			if (res)
				res->giveError(ex);
		}
	}
}

void PtConnection::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	int rc = 0;
	if (events & XEvent::WRITE_EVENT)
	{
		if (do_write() < 0)
			rc = -1;
	}
	if (events & XEvent::READ_EVENT)
	{
		if (do_read(dispatcher) < 0)
			rc = -1;
	}

	if (rc < 0)
	{
		disconnect();
	}
}

int PtConnection::on_write_timeout()
{
	do_timeout(1);
	return 0;
}

void PtConnection::do_timeout(int rw)
{
	XError* ex = NULL;
	const char *op = NULL;

	if (_state < ST_ACTIVE)
	{
		if (_ck_state > CK_INIT)
		{
			ex = new XERROR_MSG(AuthenticateTimeoutException, _info);
			op = "authenticate";
		}
		else
		{
			ex = new XERROR_MSG(ConnectTimeoutException, _info);
			op = "connect";
		}
	}
	else if (_state == ST_ACTIVE)
	{
		ex = new XERROR_MSG(MessageTimeoutException, _info);
		op = rw ? "write" : "read";
	}
	else
	{
		ex = new XERROR_MSG(CloseTimeoutException, _info);
		op = "close";
	}

	{
		Lock lock(*this);
		set_exception(ex);
	}

	if (xic_dlog_warn)
	{
		dlog("XIC.WARN", "peer=%s+%d #=%s timeout", _peer_ip, _peer_port, op);
	}

	disconnect();
}

int PtConnection::do_read(const XEvent::DispatcherPtr& dispatcher)
{
	try
	{
		XicMessagePtr msg;
		int rc = recv_msg(msg);
		if (rc <= 0)
			return rc;

		{
			Lock lock(*this);
			if (!iobuf_bstate_test(&_ib) || _ib.len > 0)
				dispatcher->readyFd(this, XEvent::READ_EVENT);
		}

		msg->unpack_body();

		int msgType = msg->msgType();
		if (msgType == 'Q')
		{
			CurrentI current(this, static_cast<Quest*>(msg.get()));
			{
				Lock lock(*this);
				if (_state >= ST_CLOSE)
					return 0;
				if (_state < ST_ACTIVE)
					throw XERROR_MSG(ProtocolException, "Unexpected Quest message");

				if (current._txid)
					++_processing;
			}

			handle_quest(_adapter, current);
		}
		else if (msgType == 'A')
		{
			AnswerPtr answer(static_cast<Answer*>(msg.get()));

			ResultIPtr res;
			{
				Lock lock(*this);
				res = _resultMap.removeResult(answer->txid());
				size_t outstanding = _resultMap.size();
				if (outstanding)
				{
					if (_msg_timeout > 0)
						_engine->getTimer()->replaceTask(this, _msg_timeout);
				}
				else
				{
					if (_state == ST_CLOSE && _processing == 0)
					{
						_state = ST_CLOSING;
						_graceful = true;
						_send_qmsg(lock, ByeMessage::create());
						_engine->getTimer()->replaceTaskLaterThan(this, closeTimeout());
					}
					else if (_msg_timeout > 0)
						_engine->getTimer()->removeTask(this);
				}
			}

			handle_answer(answer, res);
		}
		else
			assert(!"Can't reach here!");

		return 0;
	}
	catch (XError& ex)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());

		Lock lock(*this);
		LOC_HALT(&_iloc);
		set_exception(ex.clone());
	}
	catch (std::exception& ex)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());

		Lock lock(*this);
		LOC_HALT(&_iloc);
		set_exception(new XERROR_MSG(UnknownException, ex.what()));
	}
	return -1;
}

int PtConnection::do_write()
{
	Lock lock(*this);

	if (_state < ST_WAITING_HELLO)
	{
		_state = ST_WAITING_HELLO;
		_engine->getTimer()->removeTask(_writeTimeoutTask);
	}
	else if (_state >= ST_CLOSED)
	{
		return 0;
	}

	return _write_q(lock);
}

/* if error, return -1 and set _ex 
 */
int PtConnection::_write_q(Lock& lock)
{
	try 
	{
	again:
		LOC_BEGIN(&_oloc);
		while (true)
		{
			LOC_ANCHOR
			{
				bool empty = (_state < ST_ACTIVE) ? _kq.empty() : (_kq.empty() && _wq.empty());
				if (empty)
				{
					if (_msg_timeout > 0)
						_engine->getTimer()->removeTask(_writeTimeoutTask);
					LOC_PAUSE(0);
				}
			}

			_wMsg = !_kq.empty() ? _kq.front() : _wq.front();
			_ov = get_msg_iovec(_wMsg, &_ov_num, _cipher);

			LOC_ANCHOR
			{
			write_more:
				// NB: Linux seems to limit the 3rd argument of writev() 
				// to be no more than 1024 
				int count = (_ov_num < 1024) ? _ov_num : 1024;
				ssize_t rc = xnet_writev_nonblock(_fd, _ov, count);

				if (rc < 0)
				{
					if (xic_dlog_warn)
					{
						dlog("XIC.WARN", "peer=%s+%d #=xnet_writev_nonblock()=%zd, errno=%d",
							_peer_ip, _peer_port, rc, errno);
					}
					goto error;
				}

				int left = xnet_adjust_iovec(&_ov, count, rc);
				_ov_num -= (count - left);

				if (_ov_num > 0)
				{
					if (left == 0)
						goto write_more;
						
					if (_msg_timeout > 0)
						_engine->getTimer()->replaceTaskLaterThan(_writeTimeoutTask, _msg_timeout);
					LOC_PAUSE(0);
				}
			}
			free_msg_iovec(_wMsg, _ov);
			_ov = NULL;

			XicMessagePtr msg;
			_wMsg.swap(msg);
			if (!_wq.empty() && msg == _wq.front())
			{
				_wq.pop_front();
				bool isQuest = msg->isQuest();
				int64_t txid = msg->txid();

				if (isQuest && txid)
				{
					ResultIPtr result = _resultMap.findResult(txid);

					// Let other threads send waiting msgs.
					LOC_RESET(&_oloc);
					lock.release();
					result->questSent();
					lock.acquire();

					// NB: other threads may have changed the _oloc.
					goto again;
				}
			}
			else
			{
				_kq.pop_front();
			}
		}
	error:
		LOC_END(&_oloc);
		set_exception(new XERROR_FMT(SocketException,
			"xnet_writev_nonblock() failed, %s errno=%d", _info.c_str(), errno));
		return -1;
	}
	catch (XError& ex)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());

		set_exception(ex.clone());
	}
	catch (std::exception& ex)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "peer=%s+%d exception=%s", _peer_ip, _peer_port, ex.what());

		set_exception(new XERROR_MSG(UnknownException, ex.what()));
	}
	LOC_HALT(&_oloc);
	return -1;
}

PtListener::PtListener(PtEngine* engine, PtAdapter* adapter, int fd, int timeout, int close_timeout)
	: _engine(engine), _adapter(adapter), _fd(fd), _msg_timeout(timeout), _close_timeout(close_timeout)
{
}

PtListener::~PtListener()
{
	if (_fd >= 0)
		::close(_fd);
}

void PtListener::activate()
{
	_engine->getServerDispatcher()->addFd(this, _fd, XEvent::READ_EVENT | XEvent::EDGE_TRIGGER);
}

void PtListener::deactivate()
{
	_engine->getServerDispatcher()->removeFd(this);
}

static bool is_allowed(const struct sockaddr *addr, socklen_t addrlen)
{
	if (xnet_is_loopback_sockaddr(addr, addrlen))
		return true;

	return xic_allow_ips->emptyOrMatch(addr);
}

void PtListener::event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
{
	while (true)
	{
		xnet_inet_sockaddr_t peer_addr;
		socklen_t peer_len = sizeof(peer_addr);
		int sock = accept(_fd, (struct sockaddr *)&peer_addr, &peer_len);
		if (sock < 0)
		{
			if (errno == EINTR)
				continue;
			else if (errno == EAGAIN)
				break;

			if (xic_dlog_warn)
				dlog("XIC.WARN", "#=accept() failed, errno=%d %m", errno);

			dlog("XIC.ALERT", "#=PtListener::event_on_fd() fatal, shutdown the engine");
			_engine->shutdown();
			break;
		}

		if (!is_allowed((const struct sockaddr *)&peer_addr, peer_len))
		{
			if (xic_dlog_warn)
			{
				char ip[40];
				int port = xnet_get_peer_ip_port(sock, ip);
				dlog("XIC.WARN", "peer=%s+%d #=client ip not allowed", ip, port);
			}

			CheckWriter cw("FORBIDDEN");
			cw.param("reason", "ip not allowed");
			CheckPtr check = cw.take();
			int iov_num;
			struct iovec *iov = get_msg_iovec(check, &iov_num, MyCipherPtr());
			::writev(sock, iov, iov_num);
			_engine->getTimer()->addTask(XTimerTask::create(::close, sock), 1000);
			continue;
		}

		xnet_set_tcp_nodelay(sock);
		xnet_set_keepalive(sock);

		PtConnectionPtr con;
		try
		{
			con.reset(new PtConnection(_engine.get(), _adapter.get(), sock, _msg_timeout, _close_timeout));
			con->start();
			_engine->incomingConnection(con);
		}
		catch (std::exception& ex)
		{
			if (xic_dlog_warn)
				dlog("XIC.WARN", "#=PtConnection() failed, exception=%s", ex.what());

			::close(sock);
			continue;
		}
	}
}

int PtAdapter::_createListener(xic::EndpointInfo& ei, const char *ip)
{
	int real_port = ei.port;
	int fd = xnet_tcp_listen(ip, ei.port, 256);
	if (fd < 0)
		throw XERROR_FMT(SocketException, "xnet_tcp_listen() failed, address=%s+%d, errno=%d %m", ip, ei.port, errno);

	if (ei.port == 0)
	{
		char tmp[64];
		real_port = xnet_get_sock_ip_port(fd, tmp);
	}

	PtListenerPtr listener(new PtListener(_engine.get(), this, fd, ei.timeout, ei.close_timeout));
	_listeners.push_back(listener);
	return real_port;
}

void PtAdapter::_appendEndpoint(xic::EndpointInfo& ei, const char *ip, int port)
{
	std::string ep = format_string("@%.*s+%s+%d", XSTR_P(&ei.proto), ip, port);
	if (ei.timeout > 0 || ei.close_timeout > 0 || ei.connect_timeout > 0)
	{
		ep += format_string(" timeout=%d,%d,%d", ei.timeout, ei.close_timeout, ei.connect_timeout);
	}

	if (_endpoints.empty())
		_endpoints = ep;
	else
		_endpoints += " " + ep;
}

PtAdapter::PtAdapter(PtEngine* engine, const std::string& name, const std::string& endpoints)
	: _engine(engine)
{
	_state = ADAPTER_INIT;

	if (name.empty())
	{
		uuid_t uuid;
		uuid_generate(uuid);

		char buf[64];
		size_t n = 0;
		buf[n++] = '_';
		n += xbase32_encode(&buf[n], uuid, sizeof(uuid));
		_name = std::string(buf, n);

		if (endpoints.empty())
			_state = ADAPTER_ACTIVE;
	}
	else
	{
		_name = name;
	}

	xstr_t tmp = XSTR_CXX(endpoints);
	xstr_t endpoint;

	_hint = _srvMap.begin();

	this->xref_inc();
	while (xstr_delimit_char(&tmp, '@', &endpoint))
	{
		xstr_trim(&endpoint);
		if (endpoint.len == 0)
			continue;

		xic::EndpointInfo ei;
		uint32_t ip4s[32];
		uint8_t ip6s[32][16];
		int v4num = 32, v6num = 32;
		bool any = false;

		parseEndpoint(endpoint, ei);
		size_t n = getIps(ei.host, ip4s, &v4num, ip6s, &v6num, any);
		if (n == 0)
			continue;

		char ip[40];
		if (any)
		{
			int port = 0;
			if (v6num)
			{
				port = _createListener(ei, "::");
				for (int i = 0; i < v6num; ++i)
				{
					xnet_ipv6_ntoa(ip6s[i], ip);
					_appendEndpoint(ei, ip, port);
				}
			}

			if (v4num)
			{
				if (v6num == 0)
					port = _createListener(ei, "0.0.0.0");
				for (int i = 0; i < v4num; ++i)
				{
					xnet_ipv4_ntoa(ip4s[i], ip);
					_appendEndpoint(ei, ip, port);
				}
			}
		}
		else
		{
			for (int i = 0; i < v4num; ++i)
			{
				xnet_ipv4_ntoa(ip4s[i], ip);
				int port = _createListener(ei, ip); 
				_appendEndpoint(ei, ip, port);
			}

			for (int i = 0; i < v6num; ++i)
			{
				xnet_ipv6_ntoa(ip6s[i], ip);
				int port = _createListener(ei, ip); 
				_appendEndpoint(ei, ip, port);
			}
		}
	}

	if (!endpoints.empty() && _listeners.empty())
		throw XERROR_FMT(XError, "No invalid endpoints for Adapter(%s), endpoints=%s", _name.c_str(), endpoints.c_str());

	this->xref_dec_only();
}

PtAdapter::~PtAdapter()
{
}

void PtAdapter::activate()
{
	Lock lock(*this);
	if (_state >= ADAPTER_ACTIVE && _listeners.size())
		throw XERROR_FMT(XError, "Adapter(%s) already activated", _name.c_str());

	_state = ADAPTER_ACTIVE;
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		_listeners[i]->activate();
	}
}

void PtAdapter::deactivate()
{
	Lock lock(*this);
	if (_state >= ADAPTER_FINISHED)
		throw XERROR_FMT(XError, "Adapter(%s) already deactivated", _name.c_str());

	_state = ADAPTER_FINISHED;
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		_listeners[i]->deactivate();
	}
	_listeners.clear();
	_srvMap.clear();
	_default.reset();
	_hint = _srvMap.begin();
}

ProxyPtr PtAdapter::addServant(const std::string& service, Servant* servant)
{
	ProxyPtr prx;

	if (servant && !service.empty())
	{
		{
			Lock lock(*this);
			if (_state >= ADAPTER_FINISHED)
				throw XERROR_FMT(XError, "Adapter(%s) already deactivated", _name.c_str());
			_srvMap[service] = ServantPtr(servant);
			_hint = _srvMap.begin();
		}
		std::string proxy = _endpoints.empty() ? service : (service + ' ' + _endpoints);
		prx = _engine->stringToProxy(proxy);
	}
	return prx;
}

ServantPtr PtAdapter::findServant(const std::string& service) const
{
	ServantPtr srv;

	if (service.length() == 1 && service.data()[0] == 0)
	{
		srv = _engine;
	}
	else
	{
		Lock lock(*this);
		if (_hint != _srvMap.end() && _hint->first == service)
		{
			srv = _hint->second;
		}
		else
		{
			ServantMap::const_iterator iter = _srvMap.find(service);
			if (iter != _srvMap.end())
			{
				_hint = iter;
				srv = iter->second;
			}
		}
	}
	return srv;
}

ServantPtr PtAdapter::removeServant(const std::string& service)
{
	ServantPtr srv;

	Lock lock(*this);
	std::map<std::string, ServantPtr>::iterator iter = _srvMap.find(service);
	if (iter != _srvMap.end())
	{
		srv = iter->second;
		_srvMap.erase(service);
		_hint = _srvMap.begin();
	}
	return srv;
}

ServantPtr PtAdapter::getDefaultServant() const
{
	Lock lock(*this);
	return _default;
}

void PtAdapter::setDefaultServant(Servant* servant)
{
	Lock lock(*this);
	_default.reset(servant);
}

void PtAdapter::unsetDefaultServant()
{
	Lock lock(*this);
	_default.reset();
}

std::vector<std::string> PtAdapter::getServices() const
{
	std::vector<std::string> rs;

	Lock lock(*this);
	for (std::map<std::string, ServantPtr>::const_iterator iter = _srvMap.begin(); iter != _srvMap.end(); ++iter)
	{
		rs.push_back(iter->first);
	}
	return rs;
}

ContextPtr PtProxy::getContext() const
{
	Lock lock(*this);
	return _ctx;
}

void PtProxy::setContext(const ContextPtr& ctx)
{
	Lock lock(*this);
	_ctx = ctx && !ctx->empty() ? ctx: ContextPtr();
}

ConnectionPtr PtProxy::getConnection() const
{
	ConnectionPtr con;
	if (_cons.size())
	{
		Lock lock(*this);
		con = _cons[_idx];
	}
	return con;
}

void PtProxy::resetConnection()
{
	Lock lock(*this);
	for (size_t i = 0; i < _cons.size(); ++i)
		_cons[i].reset();
	_idx = 0;
}

ResultPtr PtProxy::emitQuest(const QuestPtr& quest, const CompletionPtr& completion)
{
	PtResultPtr r;
	bool twoway = quest->txid();
	if (!twoway && completion)
		throw XERROR_MSG(XLogicError, "completion callback set for oneway msg");

	xstr_t xs = XSTR_CXX(_service);
	if (xs.len && !xstr_equal(&quest->service(), &xs))
	{
		quest->setService(xs);
	}

	if (twoway)
		r.reset(new PtResult(this, quest, completion));

	try {
		ConnectionIPtr con;
		{
			Lock lock(*this);
			if (_incoming)
				con = _cons[0];
			else
				con = pickConnection(quest);

			if (_ctx && !quest->hasContext())
			{
				quest->setContext(_ctx);
			}
		}

		con->sendQuest(quest, r);
	}
	catch (XError& ex)
	{
		r->giveError(ex);
	}
	return r;
}

bool PtProxy::retryQuest(const ResultIPtr& r)
{
	const QuestPtr& quest = r->quest();
	ConnectionIPtr con;
	try {
		Lock lock(*this);
		if (!_incoming)
			con = pickConnection(quest);
	}
	catch (...)
	{
		return false;
	}

	if (!con)
		return false;

	con->sendQuest(quest, r);
	return true;
}

void PtProxy::onConnectionError(const ConnectionIPtr& con, const QuestPtr& quest)
{
	// TODO: emit the quest again using another connection.
}


static int64_t usec_diff(const struct timeval* t1, const struct timeval* t2)
{
	int64_t x1 = t1->tv_sec * 1000000 + t1->tv_usec;
	int64_t x2 = t2->tv_sec * 1000000 + t2->tv_usec;
	return x1 - x2;
}

struct PtEngine::PtThrob: public XTimerTask, private XMutex
{
	char _start_time[24];
	int _euid;
	char _euser[32];
	std::string _id;
	std::string _listen;
	std::string _logword;
	bool _enable;
	int _minute;
	struct timeval _utv;
	struct rusage _usage;

	PtThrob(time_t start, const std::string& id)
		: _euid(-1), _id(id), _enable(true), _minute(0)
	{
		dlog_local_time_str(start, _start_time);
		gettimeofday(&_utv, NULL);
		getrusage(RUSAGE_SELF, &_usage);

		_euser[0] = 0;
		_logword = engine_version_rcsid;
	}

	virtual ~PtThrob()
	{
	}

	void enable_log(bool b)
	{
		_enable = b;
	}

	void set_logword(const std::string& logword)
	{
		Lock lock(*this);
		_logword = logword;
	}

	void set_listen(const std::string& listenAddress)
	{
		Lock lock(*this);
		_listen = listenAddress;
	}

	const char *start_time() const 
	{
		return _start_time;
	}

	bool enabled() const
	{
		return _enable;
	}

	std::string logword() const
	{
		Lock lock(*this);
		return _logword;
	}

	virtual void runTimerTask(const XTimerPtr& timer)
	{
		uint64_t freq = get_cpu_frequency(0);
		int64_t now = exact_real_msec();
		int timeout = 60400 - (now % 60000);
		adjustTSC(freq);
		if (_enable)
		{
			if (timeout > 3000)
				timeout = 3000;

			int m = now / 60000;
			if (m != _minute)
			{
				_minute = m;

				struct timeval utv;
				gettimeofday(&utv, NULL);

				double cpu = 9999.9;
				struct rusage usage;
				if (getrusage(RUSAGE_SELF, &usage) == 0)
				{
					int64_t x = usec_diff(&usage.ru_utime, &_usage.ru_utime)
						 + usec_diff(&usage.ru_stime, &_usage.ru_stime);
					int64_t y = usec_diff(&utv, &_utv);
					cpu = 100.0 * x / (y > 100000 ? y : 100000);
					_utv = utv;
					_usage = usage;
				}

				int euid = geteuid();
				if (_euid != euid)
				{
					_euid = euid;
					if (unix_uid2user(_euid, _euser, sizeof(_euser)) < 0)
						snprintf(_euser, sizeof(_euser), "%d", _euid);
				}

				Lock lock(*this);
				char shadow[24];
				if (xic_passport_shadow)
					snprintf(shadow, sizeof(shadow), "%zd", xic_passport_shadow->count());
				else
					strcpy(shadow, "-");

				xdlog(NULL, NULL, "THROB", PT_ENGINE_VERSION,
					"start=%s id=%s info=euser:%s,MHz:%.0f,cpu:%.1f%%,xlog:%d,shadow:%s,cipher:%s listen=%s %s",
					_start_time, _id.c_str(),
					_euser, (freq / 1000000.0), cpu, xlog_level, shadow,
					MyCipher::get_cipher_name_from_id(xic_cipher),
					_listen.c_str(), _logword.c_str());
			}
		}

		timer->addTask(this, timeout);
	}
};


PtEngine::PtEngine(const SettingPtr& setting, const std::string& name)
	: EngineI(setting, name)
{
	if (name == "xic")
		throw XERROR_MSG(XError, "Engine name can't be \"xic\"");

	_stopped = false;
	_timer = XTimer::create();

	xref_inc();
	_throb.reset(new PtThrob(::time(NULL), _id));
	_throb->runTimerTask(_timer);
	_timer->addTask(this, CON_REAP_INTERVAL);
	_timer->start();
	xref_dec_only();
}

PtEngine::~PtEngine()
{
}

void PtEngine::setSecretBox(const SecretBoxPtr& secretBox)
{
	Lock lock(*this);
	xic_passport_secret = secretBox;
}

void PtEngine::setShadowBox(const ShadowBoxPtr& shadowBox)
{
	Lock lock(*this);
	xic_passport_shadow = shadowBox;
}

SecretBoxPtr PtEngine::getSecretBox()
{
	Lock lock(*this);
	return xic_passport_secret;
}

ShadowBoxPtr PtEngine::getShadowBox()
{
	Lock lock(*this);
	return xic_passport_shadow;
}

void PtEngine::throb(const std::string& logword)
{
	_throb->set_logword(logword);
	_throb->enable_log(true);
}

void PtEngine::_serverThreadPoolSetting(size_t& thrSize, size_t& thrMax, size_t& stackSize)
{
	ssize_t thr_min = 0, thr_max = 0, stack_size = 0;

	if (_setting)
	{
		if (!_name.empty())
		{
			thr_min = _setting->getInt(_name + ".PThreadPool.Server.Size");
			thr_max = _setting->getInt(_name + ".PThreadPool.Server.SizeMax");
			stack_size = _setting->getInt(_name + ".PThreadPool.Server.StackSize");
		}

		if (thr_min <= 0)
			thr_min = _setting->getInt("xic.PThreadPool.Server.Size", 1);
		if (thr_max <= 0)
			thr_max = _setting->getInt("xic.PThreadPool.Server.SizeMax", 1);
		if (stack_size <= 0)
			stack_size = _setting->getInt("xic.PThreadPool.Server.StackSize", DEFAULT_STACK_SIZE);
	}

	thrSize = thr_min > 0 ? thr_min : 1;
	thrMax = thr_max > (ssize_t)thrSize ? thr_max : thrSize;
	stackSize = stack_size > 0 ? stack_size : DEFAULT_STACK_SIZE;
}

void PtEngine::_clientThreadPoolSetting(size_t& thrSize, size_t& thrMax, size_t& stackSize)
{
	ssize_t thr_min = 0, thr_max = 0, stack_size = 0;

	if (_setting)
	{ 
		if (!_name.empty())
		{
			thr_min = _setting->getInt(_name + ".PThreadPool.Client.Size");
			thr_max = _setting->getInt(_name + ".PThreadPool.Client.SizeMax");
			stack_size = _setting->getInt(_name + ".PThreadPool.Client.StackSize");
		}

		if (thr_min <= 0)
			thr_min = _setting->getInt("xic.PThreadPool.Client.Size", 1);
		if (thr_max <= 0)
			thr_max = _setting->getInt("xic.PThreadPool.Client.SizeMax", 1);
		if (stack_size <= 0)
			stack_size = _setting->getInt("xic.PThreadPool.Client.StackSize", DEFAULT_STACK_SIZE);
	}

	thrSize = thr_min > 0 ? thr_min : 1;
	thrMax = thr_max > (ssize_t)thrSize ? thr_max : thrSize;
	stackSize = stack_size > 0 ? stack_size : DEFAULT_STACK_SIZE;
}

void PtEngine::_createServerDispatcher()
{
	size_t thrSize, thrMax, stackSize;
	assert(!_srvDispatcher);
	_serverThreadPoolSetting(thrSize, thrMax, stackSize);
	_srvDispatcher = XEvent::Dispatcher::create();
	_srvDispatcher->setThreadPool(thrSize, thrMax, stackSize);
	_srvDispatcher->start();
}

void PtEngine::_createClientDispatcher()
{
	size_t thrSize, thrMax, stackSize;
	assert(!_cliDispatcher);
	_clientThreadPoolSetting(thrSize, thrMax, stackSize);
	_cliDispatcher = XEvent::Dispatcher::create();
	_cliDispatcher->setThreadPool(thrSize, thrMax, stackSize);
	_cliDispatcher->start();
}

ProxyPtr PtEngine::_makeFixedProxy(const std::string& service, PtConnection* con)
{
	PtProxyPtr prx;

	Lock lock(*this);
	if (_stopped)
		throw XERROR(EngineStoppedException);

	ProxyMap::iterator iter = _proxyMap.find(service);
	if (iter != _proxyMap.end())
	{
		prx = iter->second;
		if (prx->getConnection().get() == con)
			return prx;

		_proxyMap.erase(iter);
	}

	prx.reset(new PtProxy(service, this, con));
	_proxyMap.insert(std::make_pair(service, prx));
	return prx;
}

ProxyPtr PtEngine::stringToProxy(const std::string& proxy)
{
	PtProxyPtr prx;
	{
		Lock lock(*this);
		if (_stopped)
			throw XERROR(EngineStoppedException);

		ProxyMap::iterator iter = _proxyMap.find(proxy);
		if (iter != _proxyMap.end())
		{
			prx = iter->second;
		}
		else
		{
			prx.reset(new PtProxy(proxy, this, NULL));
			_proxyMap.insert(std::make_pair(proxy, prx));
		}
	}
	return prx;
}

void PtEngine::runTimerTask(const XTimerPtr& timer)
{
	time_t before;
	time_t now = this->time();
	bool shadow_changed = false;

	if (xic_passport_secret)
	{
		SecretBoxPtr sb = xic_passport_secret->reload();
		if (sb)
		{
			Lock lock(*this);
			xic_passport_secret.swap(sb);
		}
	}

	if (xic_passport_shadow)
	{
		ShadowBoxPtr sb = xic_passport_shadow->reload();
		if (sb)
		{
			shadow_changed = true;
			Lock lock(*this);
			xic_passport_shadow.swap(sb);
		}
	}

	Lock lock(*this);

	before = xic_acm_client > 0 ? now - xic_acm_client : 0;
	for (ConnectionMap::iterator iter = _conMap.begin(); iter != _conMap.end(); )
	{
		if (iter->second->reap_idle(now, before))
		{
			_conMap.erase(iter++);
		}
		else
		{
			++iter;
		}
	}

	if (shadow_changed)
	{
		ConnectionList cons;
		_incomingCons.swap(cons);
		lock.release();

		for (ConnectionList::iterator iter = cons.begin(); iter != cons.end(); ++iter)
		{
			(*iter)->close(false);
		}
	}
	else
	{
		before = xic_acm_server > 0 ? now - xic_acm_server : 0;
		for (ConnectionList::iterator iter = _incomingCons.begin(); iter != _incomingCons.end(); )
		{
			if ((*iter)->reap_idle(now, before))
			{
				iter = _incomingCons.erase(iter);
			}
			else
			{
				++iter;
			}
		}
		lock.release();
	}

	timer->addTask(this, CON_REAP_INTERVAL);
}

void PtEngine::incomingConnection(const PtConnectionPtr& con)
{
	Lock lock(*this);
	_incomingCons.push_back(con);
}

ConnectionIPtr PtEngine::makeConnection(const std::string& service, const std::string& endpoint)
{
	PtConnectionPtr con;
	{
		Lock lock(*this);
		if (_stopped)
			throw XERROR(EngineStoppedException);

		int gen = 0;
		ConnectionMap::iterator iter = _conMap.find(endpoint);
		if (iter != _conMap.end())
		{
			if (isLive(iter->second->state()))
				return iter->second;

			gen = iter->second->attempt() + 1;
		}

		if (!_cliDispatcher)
			_createClientDispatcher();

		con.reset(new PtConnection(this, service, endpoint, gen));
		if (con)
		{
			_conMap[endpoint] = con;
			con->start();
		}
	}

	return con;
}


AdapterPtr PtEngine::createAdapter(const std::string& adapterName, const std::string& endpoints)
{
	const std::string& name = adapterName.empty() ? "xic" : adapterName;

	if (_stopped)
		throw XERROR(EngineStoppedException);

	std::string eps = endpoints;
	if (eps.empty() && _setting)
		eps = _setting->wantString(name + ".Endpoints");

	if (eps.empty())
		throw XERROR_FMT(XError, "No endpoints for Adapter(%s)", name.c_str());

	Lock lock(*this);
	if (_stopped)
		throw XERROR(EngineStoppedException);

	if (!_srvDispatcher)
		_createServerDispatcher();

	PtAdapterPtr adapter(new PtAdapter(this, name, eps));
	if (adapter)
	{
		if (!_adapterMap.insert(std::make_pair(name, adapter)).second)
			throw XERROR_FMT(XError, "Adapter(%s) already created", name.c_str());
	}

	std::ostringstream ss;
	for (AdapterMap::iterator iter = _adapterMap.begin(); iter != _adapterMap.end(); ++iter)
	{
		xstr_t eps = XSTR_CXX(iter->second->endpoints());
		xstr_t ep;
		while (xstr_delimit_char(&eps, '@', &ep))
		{
			xstr_trim(&ep);
			if (ep.len == 0)
				continue;

			xstr_t addr;
			xstr_token_space(&ep, &addr);
			ss << '@' << addr;
		}
	}
	_throb->set_listen(ss.str());

	return adapter;
}

AdapterPtr PtEngine::createSlackAdapter()
{
	if (_stopped)
		throw XERROR(EngineStoppedException);

	PtAdapterPtr adapter(new PtAdapter(this, "", ""));
	if (adapter)
	{
		Lock lock(*this);
		if (_stopped)
			throw XERROR(EngineStoppedException);

		if (!_adapterMap.insert(std::make_pair(adapter->name(), adapter)).second)
			throw XERROR_FMT(XError, "Adapter(%s) already created", adapter->name().c_str());
	}
	return adapter;
}

time_t PtEngine::time()
{
	return exact_real_msec() / 1000;
}

int PtEngine::sleep(int seconds)
{
	return ::sleep(seconds);
}

static void *wait_and_exit(void *arg)
{
	int seconds = (intptr_t)arg;
	::sleep(seconds);
	exit(1);
}

void PtEngine::_doom(int seconds)
{
	try {
		XThread::create(wait_and_exit, (void *)(intptr_t)seconds, false, 1024*256);
	}
	catch (...)
	{
		exit(3);
	}
}

void PtEngine::waitForShutdown()
{
	xic::readyToServe(_setting);

	_timer->waitForCancel();

	_proxyMap.clear();
}

int PtEngine::check_stop()
{
	if (_stopped)
	{
		int numFd = 0;
		if (_srvDispatcher)
			numFd += _srvDispatcher->countFd();
		if (_cliDispatcher)
			numFd += _cliDispatcher->countFd();

		if (numFd > 0)
			return 919;	// random less than 1000 milliseconds

		_timer->cancel();
		return 0;
	}
	return 0;
}

void PtEngine::shutdown()
{
	ProxyMap proxyMap;
	AdapterMap adapterMap;
	ConnectionList incomingCons;
	ConnectionMap conMap;

	{
		Lock lock(*this);
		if (_stopped)
			return;
		_stopped = true;
		_proxyMap.swap(proxyMap);
		_adapterMap.swap(adapterMap);
		_incomingCons.swap(incomingCons);
		_conMap.swap(conMap);
	}

	for (AdapterMap::iterator iter = adapterMap.begin(); iter != adapterMap.end(); ++iter)
	{
		iter->second->deactivate();
	}

	for (ConnectionList::iterator iter = incomingCons.begin(); iter != incomingCons.end(); ++iter)
	{
		(*iter)->close(false);
	}

	for (ConnectionMap::iterator iter = conMap.begin(); iter != conMap.end(); ++iter)
	{
		iter->second->close(false);
	}

	int msec = check_stop();
	if (msec > 0)
		_timer->addTask(XTimerTask::create(this, &PtEngine::check_stop), msec);

	// wait at most SHUTDOWN_WAIT seconds
	_doom(SHUTDOWN_WAIT);
}

void PtEngine::_info(AnswerWriter& aw)
{
	Lock lock(*this);
	aw.param("engine.start_time", _throb->start_time());
	aw.param("engine.type", "PtEngine");
	aw.param("engine.version", PT_ENGINE_VERSION);
	aw.param("throb.enabled", _throb->enabled());
	aw.param("throb.logword", _throb->logword());

	size_t srv_size = 0, srv_sizemax = 0, srv_stacksize = 0;
	if (_srvDispatcher)
		_srvDispatcher->getThreadPool(&srv_size, &srv_sizemax, &srv_stacksize);
	else
		_serverThreadPoolSetting(srv_size, srv_sizemax, srv_stacksize);
	aw.param("threadpool.server.current", _srvDispatcher ? _srvDispatcher->countThread() : 0);
	aw.param("threadpool.server.size", srv_size);
	aw.param("threadpool.server.sizemax", srv_sizemax);
	aw.param("threadpool.server.stacksize", srv_stacksize);

	size_t cli_size = 0, cli_sizemax = 0, cli_stacksize = 0;
	if (_cliDispatcher)
		_cliDispatcher->getThreadPool(&cli_size, &cli_sizemax, &cli_stacksize);
	else
		_clientThreadPoolSetting(cli_size, cli_sizemax, cli_stacksize);
	aw.param("threadpool.client.current", _cliDispatcher ? _cliDispatcher->countThread() : 0);
	aw.param("threadpool.client.size", cli_size);
	aw.param("threadpool.client.sizemax", cli_sizemax);
	aw.param("threadpool.client.stacksize", cli_stacksize);

	aw.param("adapter.count", _adapterMap.size());
	aw.param("proxy.count", _proxyMap.size());
	aw.param("connection.count", _conMap.size());

	VListWriter lw;

	lw = aw.paramVList("adapters");
	for (AdapterMap::iterator iter = _adapterMap.begin(); iter != _adapterMap.end(); ++iter)
	{
		PtAdapterPtr& adapter = iter->second;

		VDictWriter dw = lw.vdict();
		dw.kv("endpoints", adapter->endpoints());
		std::vector<std::string> services = adapter->getServices();
		VListWriter llw = dw.kvlist("services");
		size_t size = services.size();
		for (size_t i = 0; i < size; ++i)
		{
			llw.v(services[i]);
		}
		dw.kv("catchall", bool(adapter->getDefaultServant()));
	}

	lw = aw.paramVList("proxies");
	for (ProxyMap::iterator iter = _proxyMap.begin(); iter != _proxyMap.end(); ++iter)
	{
		lw.v(iter->second->str());
	}

	char buf[256];
	buf[0] = 0;
	if (xic_passport_secret)
	{
		SecretBoxPtr& s = xic_passport_secret;
		snprintf(buf, sizeof(buf), "%s|%zd", s->filename().c_str(), s->count());
	}
	aw.param("xic.passport.secret", buf);

	buf[0] = 0;
	if (xic_passport_shadow)
	{
		ShadowBoxPtr& s = xic_passport_shadow;
		snprintf(buf, sizeof(buf), "%s|%zd", s->filename().c_str(), s->count());
	}
	aw.param("xic.passport.shadow", buf);
}

static void *sig_thread(void *arg)
{
	sigset_t sset;
	sigfillset(&sset);

	while (true)
	{
		int sig;
		int rc = sigwait(&sset, &sig);
		if (rc != 0)
			throw XERROR_FMT(XError, "sigwait()=%d", rc);

		if (xic_dlog_warn)
			dlog("XIC.WARN", "#=signal(%d) catched", sig);

		if (xic_engine)
			xic_engine->shutdown();
		else
			exit(1);
	}
}

static void sig_fpe(int sig)
{
	xlog(XLOG_ALERT, "SIGFPE received: st_thread_t is probably created in PtEngine. exiting.");
	exit(1);
}

EnginePtr Engine::pt(const SettingPtr& setting, const std::string& name)
{
	if (xic_engine)
		throw XERROR_MSG(XLogicError, "Only one instance of xic::Engine is allowed in a process");

	signal(SIGFPE, sig_fpe);

	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, SIGINT);
	sigaddset(&sset, SIGTERM);
	int rc;
	rc = pthread_sigmask(SIG_BLOCK, &sset, NULL);
	if (rc != 0)
		throw XERROR_FMT(XError, "pthread_sigmask()=%d", rc);

	prepareEngine(setting);
	xic_engine.reset(new PtEngine(setting, name));

	pthread_t thr;
	rc = pthread_create(&thr, NULL, sig_thread, NULL);
	if (rc != 0)
		throw XERROR_FMT(XError, "pthread_create()=%d", rc);
	pthread_detach(thr);

	return xic_engine;
}


class PtApp: public ApplicationI
{
	xic_application_function _func;
public:
	PtApp(xic_application_function func)
		: ApplicationI(xic::Engine::pt), _func(func)
	{
	}

	int run(int argc, char **argv)
	{
		return _func(argc, argv, engine());
	}
};

int xic::start_xic_pt(xic_application_function func, int argc, char **argv, const SettingPtr& setting)
{
	xlog_level = XLOG_WARN;
	PtApp app(func);
	return app.main(argc, argv, setting);
}

