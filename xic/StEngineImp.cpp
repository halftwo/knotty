#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif
#include "StEngineImp.h"
#include "sthread.h"
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
#include "xslib/rdtsc.h"
#include "xslib/ScopeGuard.h"
#include "xslib/unix_user.h"
#include "xslib/msec.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <utility>
#include <sstream>

#define DEFAULT_THR_MAX		10000
#define DEFAULT_STACK_SIZE	(64*1024)
#define CON_REAP_INTERVAL	(17*1000)	// msec
#define SHUTDOWN_WAIT		15		// seconds

#define ST_ENGINE_VERSION	"ST." XIC_VERSION

static char engine_version_rcsid[] = "$xic: " ST_ENGINE_VERSION " $";

using namespace xic;


obpool_t StResult::_pool = OBPOOL_INITIALIZER(sizeof(StResult));


StResult::StResult(ProxyI* prx, const QuestPtr& quest, const CompletionPtr& completion)
	: ResultI(prx, quest, completion)
{
	_cond = !completion ? st_cond_new() : 0;
}

StResult::~StResult()
{
	if (_cond)
		st_cond_destroy(_cond);
}

bool StResult::isSent() const
{
	return _isSent;
}

void StResult::waitForSent()
{
	_waitSent = true;
	if (_cond)
	{
		while (!_isSent)
		{
			int r = st_cond_wait(_cond);
			if (r == -1)
			{
				if (errno == EINTR)
				{
					throw XERROR(InterruptException);
				}
				else
				{
					throw XERROR_FMT(XSyscallError, "errno=%d", errno);
				}
			}
		}
	}
}

bool StResult::isCompleted() const
{
	return (_answer || _ex.get());
}

void StResult::waitForCompleted()
{
	_waitCompleted = true;
	if (_cond)
	{
		while (!_answer && !_ex.get())
		{
			int r = st_cond_wait(_cond);
			if (r == -1)
			{
				if (errno == EINTR)
				{
					throw XERROR(InterruptException);
				}
				else
				{
					throw XERROR_FMT(XSyscallError, "errno=%d", errno);
				}
			}
		}
	}
}

AnswerPtr StResult::takeAnswer(bool throw_ex)
{
	waitForCompleted();

	if (_ex.get())
	{
		if (throw_ex)
		{
			_ex->do_throw();
		}
		else
		{
			return except2answer(*_ex, _quest->method(), _quest->service(), _con ? _con->endpoint() : "", true);
		}
	}

	AnswerPtr r;
	_answer.swap(r);

	if (r)
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

void StResult::_notify()
{
	if (_completion)
	{
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
	else if (_cond && _waitCompleted)
	{
		st_cond_broadcast(_cond);
	}
}

bool StResult::retry()
{
	if (_retryNumber > 0)
		return false;

	++_retryNumber;
	_isSent = false;
	return _prx->retryQuest(this);
}

void StResult::giveError(const XError& ex)
{
	assert(!_answer && !_ex.get());

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

	_ex.reset(ex.clone());
	_notify();
}

void StResult::giveAnswer(AnswerPtr& answer)
{
	assert(!_answer && !_ex.get());

	if (!_cond && !_completion)
		throw XERROR_MSG(XLogicError, "Oneway request should NOT have an Answer");	

	_answer.swap(answer);
	_notify();
}

void StResult::questSent()
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
	else if (_cond && _waitSent)
	{
		st_cond_broadcast(_cond);
	}
}

void StConnection::init()
{
	_recv_cond = st_cond_new();
	_send_cond = st_cond_new();
	_recent_active = true;
	_active_time = 0;

	_send_sth = sthread_create(this, &StConnection::send_fiber, 0, 
			_incoming ? _engine->serverStackSize() : _engine->clientStackSize());
	_recv_sth = NULL;

	if (_msg_timeout <= 0 && xic_timeout_message > 0)
		_msg_timeout = xic_timeout_message;

	if (_msg_timeout > 0)
	{
		_writeTimeoutTask = STimerTask::create(this, &StConnection::on_write_timeout);
	}
}

StConnection::StConnection(StEngine* engine, const std::string& service, const std::string& endpoint, int attempt)
	: ConnectionI(false, attempt, 0, 0, 0), _engine(engine), _sf(NULL)
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

	/* XXX: this may block the whole process if host is domain instead of IP */
	xnet_inet_sockaddr_t addr;
	if (xnet_ip46_sockaddr(buf, ei.port, (struct sockaddr *)&addr) < 0)
		throw XERROR_FMT(XError, "xnet_ip46_sockaddr() failed, host=%s", buf);

	int sock = xnet_tcp_connect_sockaddr_nonblock((struct sockaddr *)&addr, sizeof(addr));
	if (sock < 0)
		throw XERROR_FMT(XSyscallError, "xnet_tcp_connect_sockaddr_nonblock() failed, addr=%s+%d errno=%d %m", buf, ei.port, errno);

	xnet_set_tcp_nodelay(sock);
	xnet_set_keepalive(sock);

	_sock_port = xnet_get_sock_ip_port(sock, _sock_ip);
	if (addr.family == AF_INET6)
		xnet_ipv6_ntoa(addr.a6.sin6_addr.s6_addr, _peer_ip);
	else
		xnet_ipv4_ntoa(ntohl(addr.a4.sin_addr.s_addr), _peer_ip);
        _peer_port = ei.port;

	char info[128];
	int len = snprintf(info, sizeof(info), "%s/%s+%d/%s+%d", _proto.c_str(), _sock_ip, _sock_port, _peer_ip, _peer_port);
	_info = std::string(info, len);

	_sf = st_netfd_open_socket(sock);

	this->init();
	xref_dec_only();
}

StConnection::StConnection(StEngine* engine, StAdapter* adapter, st_netfd_t sf,
	 		int timeout, int close_timeout)
	: ConnectionI(true, 0, timeout, close_timeout, 0),
	 _engine(engine), _sf(sf)
{
	xref_inc();
	_adapter.reset(adapter);
	int fd = st_netfd_fileno(sf);
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
		dlog("XIC.DEBUG", "peer=%s+%d #=Connection accepted", _peer_ip, _peer_port);

	char info[128];
	int len = snprintf(info, sizeof(info), "%s/%s+%d/%s+%d", _proto.c_str(), _sock_ip, _sock_port, _peer_ip, _peer_port);
	_info = std::string(info, len);

	this->init();

	_shadowBox = _engine->getShadowBox();
	if (_shadowBox && !_shadowBox->empty())
	{
		CheckWriter cw("AUTHENTICATE");
		cw.param("method", "SRP6a");
		_state = ST_WAITING_HELLO;
		_ck_state = CK_S1;
		send_kmsg(cw.take());
		_engine->getTimer()->replaceTask(this, connectTimeout());
	}
	else
	{
		XicMessagePtr msg = HelloMessage::create();
		_ck_state = CK_FINISH;
		_state = ST_ACTIVE;
		_send_qmsg(msg);
	}
	xref_dec_only();
}

StConnection::~StConnection()
{
	disconnect();

	st_cond_destroy(_recv_cond);
	st_cond_destroy(_send_cond);
}

void StConnection::set_exception(XError *ex)
{
	_state = ST_ERROR;
	_ex_time = _engine->time();
	_ex.reset(ex);
}

void StConnection::_grace()
{
	if (_state < ST_CLOSE)
	{
		_state = ST_CLOSE;
		if (_processing == 0 && _resultMap.size() == 0)
		{
			_state = ST_CLOSING;
			_graceful = true;
			_send_qmsg(ByeMessage::create());
			_engine->getTimer()->replaceTaskLaterThan(this, closeTimeout());
		}
	}
}

bool StConnection::reap_idle(time_t now, time_t before)
{
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
		_grace();
		return true;
	}

	return false;
}

void StConnection::close(bool force)
{
	if (force)
	{
		disconnect();
	}
	else
	{
		_grace();
	}
}

ProxyPtr StConnection::createProxy(const std::string& service)
{
	if (service.find('@') != std::string::npos)
		throw XERROR_MSG(ServiceParseException, service);

	return _engine->_makeFixedProxy(service, this);
}

int StConnection::disconnect()
{
	if (_state < ST_CLOSED)
		_state = ST_CLOSED;

	if (_recv_sth)
		st_thread_interrupt(_recv_sth);
	if (_send_sth)
		st_thread_interrupt(_send_sth);

	_engine->getTimer()->removeTask(this);
	if (_writeTimeoutTask)
	{
		_engine->getTimer()->removeTask(_writeTimeoutTask);
		_writeTimeoutTask.reset();
	}

	if (_sf && st_netfd_close(_sf) == 0)
	{
		_sf = NULL;
		_adapter.reset();
	}

	std::map<int64_t, ResultIPtr> theMap;

	_wq.clear();
	_resultMap.take(theMap);

	if (theMap.size())
	{
		UniquePtr<XError> ex;
		if (_ex.get())
			ex.reset(_ex->clone());
		else
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

int StConnection::on_write_timeout()
{
	do_timeout(1);
	return 0;
}

void StConnection::do_timeout(int rw)
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

	set_exception(ex);

	if (xic_dlog_warn)
	{
		dlog("XIC.WARN", "peer=%s+%d #=%s timeout", _peer_ip, _peer_port, op);
	}

	if (_recv_sth)
		st_thread_interrupt(_recv_sth);
	if (_send_sth)
		st_thread_interrupt(_send_sth);
}

static int _check_io_result(int n, const Connection::State& state, const std::string& conInfo)
{
	if (n == -1)
	{
		if (errno == EINTR)
			throw XERROR_MSG(InterruptException, "Interrupt");
		else
			throw XERROR_FMT(XSyscallError, "errno=%d", errno);
	}

	if (state >= Connection::ST_CLOSING)
		return -1;
	throw XERROR_MSG(ConnectionLostException, conInfo);
}

int StConnection::recv_msg(XicMessagePtr& msg)
{
	XicMessage::Header hdr;
	int n;

	n = st_read_fully(_sf, &hdr, sizeof(hdr), -1);
	_recent_active = true;
	if (n < (int)sizeof(hdr))
	{
		if (n == -1)
		{
			if (errno == EINTR)
				throw XERROR_MSG(InterruptException, "Interrupt");
			else
				throw XERROR_FMT(XSyscallError, "errno=%d", errno);
		}

		if (_state >= ST_CLOSING)
			return -1;
		if (_state >= ST_ACTIVE)
			throw XERROR_MSG(ConnectionLostException, _info);
		else if (_ck_state > CK_INIT)
			throw XERROR_MSG(AuthenticationException, _info);
		else
			throw XERROR_MSG(ConnectFailedException, _info);
	}

	if (hdr.magic != 'X')
		throw XERROR_FMT(ProtocolException, "%s Invalid packet header magic %#x", _info.c_str(), hdr.magic);

	if (hdr.version != '!')
		throw XERROR_FMT(ProtocolException, "%s Unknown packet version %#x", _info.c_str(), hdr.version);

	if (hdr.flags & ~XIC_FLAG_MASK)
		throw XERROR_FMT(ProtocolException, "%s Unknown packet header flag %#x", _info.c_str(), hdr.flags);

	int flagCipher = (hdr.flags & (XIC_FLAG_CIPHER_MODE0 | XIC_FLAG_CIPHER_MODE1));
	if (flagCipher && !_cipher)
	{
		throw XERROR_FMT(ProtocolException, "%s CIPHER flag set but No CIPHER negociated before", _info.c_str());
	}

	if (flagCipher == (XIC_FLAG_CIPHER_MODE0 | XIC_FLAG_CIPHER_MODE1))
	{
		throw XERROR_FMT(ProtocolException, "%s CIPHER flag mode0 and mode1 both set", _info.c_str());
	}
	else if (flagCipher == XIC_FLAG_CIPHER_MODE0)
	{
		throw XERROR_FMT(ProtocolException, "%s CIPHER mode0 not supported", _info.c_str());
	}

	uint32_t bodySize = xnet_m32(hdr.bodySize);
	if (bodySize > xic_message_size)
		throw XERROR_FMT(MessageSizeException, "%s Huge packet bodySize %lu>%u", 
				_info.c_str(), (unsigned long)bodySize, xic_message_size);

	if (hdr.msgType == 'H')
	{
		if (bodySize != 0)
			throw XERROR_FMT(ProtocolException, "%s Invalid hello message", _info.c_str());

		if (_state < ST_ACTIVE)
		{
			_state = ST_ACTIVE;
			if (_resultMap.size() == 0)
				_engine->getTimer()->removeTask(this);

			if (!_wq.empty())
				st_cond_signal(_send_cond);
		}
		return 0;
	}
	else if (hdr.msgType == 'B')
	{
		if (bodySize != 0)
			throw XERROR_FMT(ProtocolException, "%s Invalid close message", _info.c_str());

		if (xic_dlog_debug)
			dlog("XIC.DEBUG", "peer=%s+%d #=Close message received", _peer_ip, _peer_port);

		_state = ST_CLOSED;
		_graceful = true;
		return -1;
	}

	if (flagCipher)
	{
		if (bodySize <= _cipher->extraSize())
			throw XERROR_FMT(MessageSizeException, "%s Invalid packet bodySize %lu", 
					_info.c_str(), (unsigned long)bodySize);

		bodySize -= _cipher->extraSize();
	}

	msg = XicMessage::create(hdr.msgType, bodySize);

	xstr_t body = msg->body();
	n = st_read_fully(_sf, body.data, body.len, -1);
	_recent_active = true;
	if (n < (int)bodySize)
	{
		return _check_io_result(n, _state, _info);
	}

	if (flagCipher)
	{
		n = st_read_fully(_sf, _cipher->iMAC, sizeof(_cipher->iMAC), -1);
		if (n < (int)sizeof(_cipher->iMAC))
		{
			return _check_io_result(n, _state, _info);
		}

		_cipher->decryptStart(&hdr, sizeof(hdr));
		_cipher->decryptUpdate(body.data, body.data, body.len);
		bool ok = _cipher->decryptFinish();
		if (!ok)
			throw XERROR_FMT(ProtocolException, "Msg body failed to decrypt, %s", _info.c_str());
	}

	if (hdr.msgType == 'C')
	{
		if (_state != ST_WAITING_HELLO)
			throw XERROR_MSG(ProtocolException, "Unexpected Check message");

		msg->unpack_body();
		CheckPtr check(static_cast<Check*>(msg.get()));
		msg.reset();
		try {
			handle_check(check);
		}
		catch (XError& ex)
		{
			if (!_incoming)
				throw;

			set_exception(ex.clone());
			_engine->getTimer()->addTask(STimerTask::create(this, &StConnection::disconnect), 1000);
		}
		return 0;
	}

	return 1;
}
 
SecretBoxPtr StConnection::getSecretBox()
{
	return _engine->getSecretBox();
}

void StConnection::checkFinished()
{
	_engine->getTimer()->removeTask(this);

	if (_state < ST_ACTIVE)
		_state = ST_ACTIVE;

	if (!_wq.empty())
		st_cond_signal(_send_cond);
}

void StConnection::send_kmsg(const XicMessagePtr& msg)
{
	ENFORCE(_state < ST_ACTIVE);
	_kq.push_back(msg);
	if (_sf)
		st_cond_signal(_send_cond);
}

void StConnection::_send_qmsg(const XicMessagePtr& msg)
{
	_recent_active = true;
	if (_state < ST_CLOSED)
	{
		_wq.push_back(msg);
		if (_sf && _state >= ST_ACTIVE)
			st_cond_signal(_send_cond);
	}
	else if (xic_dlog_warn)
	{
		dlog("XIC.WARN", "peer=%s+%d #=send msg (type:%d) to closed connection", _peer_ip, _peer_port, msg->msgType());
	}
}

void StConnection::replyAnswer(const AnswerPtr& answer)
{
	_send_qmsg(answer);
	--_processing;
	if (_state == ST_CLOSE && _processing == 0 && _resultMap.size() == 0)
	{
		_state = ST_CLOSING;
		_graceful = true;
		_send_qmsg(ByeMessage::create());
		_engine->getTimer()->replaceTaskLaterThan(this, closeTimeout());
	}
}

void StConnection::sendQuest(const QuestPtr& quest, const ResultIPtr& r)
{
	quest->reset_iovec();
	bool twoway = bool(r);
	try
	{
		if (twoway)
		{
			assert(quest == r->quest());
			r->setConnection(this);

			if (_state >= ST_CLOSE)
			{
				throw XERROR_MSG(ConnectionClosedException, _info);
			}

			size_t outstanding = _resultMap.size();
			quest->setTxid(_resultMap.addResult(r));
			if (_msg_timeout > 0 && outstanding == 0)
			{
				_engine->getTimer()->replaceTaskLaterThan(this, _msg_timeout);
			}
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

		_send_qmsg(quest);
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

void StConnection::recv_fiber()
{
	if (!_sf)
	{
		goto done;
	}

	try
	{
		XicMessagePtr msg;
		int rc = recv_msg(msg);
		if (rc < 0)
		{
			// Gracefully closed.
			goto done;
		}

		if (_incoming)
			_recv_sth = _engine->create_server_thread(this, &StConnection::recv_fiber);
		else
			_recv_sth = _engine->create_client_thread(this, &StConnection::recv_fiber);

		if (rc == 0)
			goto done;

		msg->unpack_body();

		int msgType = msg->msgType();
		if (msgType == 'Q')
		{
			if (_state < ST_CLOSE)
			{
				if (_state < ST_ACTIVE)
					throw XERROR_MSG(ProtocolException, "Unexpected Quest message");

				CurrentI current(this, static_cast<Quest*>(msg.get()));

				if (current._txid)
					++_processing;
				handle_quest(_adapter, current);
			}
		}
		else if (msgType == 'A')
		{
			AnswerPtr answer(static_cast<Answer*>(msg.get()));
			ResultIPtr res = _resultMap.removeResult(answer->txid());
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
					_send_qmsg(ByeMessage::create());
					_engine->getTimer()->replaceTaskLaterThan(this, closeTimeout());
				}
				else if (_msg_timeout > 0)
					_engine->getTimer()->removeTask(this);
			}

			handle_answer(answer, res);
		}
		else
			assert(!"Can't reach here!");
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
done:
	if (_recv_sth == st_thread_self())
	{
		_recv_sth = 0;
		disconnect();
	}
}

void StConnection::send_fiber()
{
	assert(_sf);

	if (_incoming)
	{
		_recv_sth = _engine->create_server_thread(this, &StConnection::recv_fiber);
	}
	else
	{
		if (st_netfd_poll(_sf, POLLOUT, (st_utime_t)1000 * connectTimeout()) < 0
			|| xnet_get_so_error(st_netfd_fileno(_sf)) > 0)
		{
			st_netfd_close(_sf);
			_sf = NULL;
			set_exception(new XERROR_MSG(ConnectFailedException, _info));
			goto error;
		}
		else
		{
			if (_state < ST_WAITING_HELLO)
				_state = ST_WAITING_HELLO;

			_recv_sth = _engine->create_client_thread(this, &StConnection::recv_fiber);
		}
	}

	try
	{
		while (true)
		{
			while (_state < ST_ACTIVE ? _kq.empty() : (_kq.empty() && _wq.empty()))
			{
				if (st_cond_wait(_send_cond) < 0)
					goto error;
			}

			if (_msg_timeout > 0)
				_engine->getTimer()->addTask(_writeTimeoutTask, _msg_timeout);

			MessageQueue& q = !_kq.empty() ? _kq : _wq;
			do
			{
				int iov_cnt = 0;
				struct iovec *iov = get_msg_iovec(q.front(), &iov_cnt, _cipher);

				// NB: The 3rd argument of writev() in Linux 
				// must be no more than 1024, or error occured.
				while (iov_cnt > 0)
				{
					int count = iov_cnt < 1024 ? iov_cnt : 1024;
					int n = st_writev(_sf, iov, count, -1);
					if (n < 0)
						throw XERROR_FMT(XSyscallError, "errno=%d", errno);
					int left = xnet_adjust_iovec(&iov, count, n);
					iov_cnt -= (count - left);
				}
				free_msg_iovec(q.front(), iov);

				XicMessage* msg = q.front().get();
				if (msg->isQuest() && msg->txid())
				{
					_resultMap.findResult(msg->txid())->questSent();
				}
				q.pop_front();
			} while (!q.empty());

			if (_msg_timeout > 0)
				_engine->getTimer()->removeTask(_writeTimeoutTask);
		}
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

error:
	_send_sth = 0;
	disconnect();
}

StListener::StListener(StEngine* engine, StAdapter* adapter, int fd, int timeout, int close_timeout)
	: _engine(engine), _adapter(adapter), _msg_timeout(timeout),
	 _close_timeout(close_timeout), _accept_thr(NULL)
{
	_sf = st_netfd_open(fd);
	if (!_sf)
	{
		::close(fd);
		throw XERROR_MSG(XError, "st_netfd_open() failed");
	}
}

StListener::~StListener()
{
	if (_sf)
		st_netfd_close(_sf);
}

void StListener::activate()
{
	if (!_accept_thr)
	{
		_accept_thr = sthread_create(this, &StListener::accept_fiber, 1, _engine->serverStackSize());
	}
}

void StListener::deactivate()
{
	if (_accept_thr)
	{
		st_thread_interrupt(_accept_thr);
	}
}

static bool is_allowed(const struct sockaddr *addr, socklen_t addrlen)
{
	if (xnet_is_loopback_sockaddr(addr, addrlen))
		return true;

	return xic_allow_ips->emptyOrMatch(addr);
}

void StListener::accept_fiber()
{
	while (true)
	{
		xnet_inet_sockaddr_t peer_addr;
		int peer_len = sizeof(peer_addr);
		st_netfd_t sf = st_accept(_sf, (struct sockaddr *)&peer_addr, &peer_len, ST_UTIME_NO_TIMEOUT);
		if (sf == NULL)
		{
			if (xic_dlog_warn)
			{
				dlog("XIC.WARN", "#=st_accept() failed, errno=%d %m", errno);
			}

			if (errno != EINTR)
			{
				dlog("XIC.ALERT", "#=StListener::accept_fiber() exit, shutdown the engine");
				_engine->shutdown();
			}
			break;
		}

		int fd = st_netfd_fileno(sf);
		if (!is_allowed((const struct sockaddr *)&peer_addr, peer_len))
		{
			if (xic_dlog_warn)
			{
				char ip[40];
				int port = xnet_get_peer_ip_port(fd, ip);
				dlog("XIC.WARN", "peer=%s+%d #=client ip not allowed", ip, port);
			}

			CheckWriter cw("FORBIDDEN");
			CheckPtr check = cw.take();
			int iov_num;
			struct iovec *iov = get_msg_iovec(check, &iov_num, MyCipherPtr());
			st_writev(sf, iov, iov_num, -1);
			st_netfd_close(sf);
			_engine->getTimer()->addTask(STimerTask::create(st_netfd_close, sf), 1000);
			continue;
		}

		xnet_set_tcp_nodelay(fd);
		xnet_set_keepalive(fd);

		try
		{
			StConnectionPtr con(new StConnection(_engine.get(), _adapter.get(), sf, _msg_timeout, _close_timeout));
			_engine->incomingConnection(con);
		}
		catch (std::exception& ex)
		{
			if (xic_dlog_warn)
				dlog("XIC.WARN", "#=StConnection() failed, exception=%s", ex.what());

			st_netfd_close(sf);
		}
	}
	st_netfd_close(_sf);
	_sf = NULL;
	_accept_thr = NULL;
}

int StAdapter::_createListener(xic::EndpointInfo& ei, const char *ip)
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

	StListenerPtr listener(new StListener(_engine.get(), this, fd, ei.timeout, ei.close_timeout));
	_listeners.push_back(listener);
	return real_port;
}

void StAdapter::_appendEndpoint(xic::EndpointInfo& ei, const char *ip, int port)
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

StAdapter::StAdapter(StEngine* engine, const std::string& name, const std::string& endpoints)
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

StAdapter::~StAdapter()
{
}

void StAdapter::activate()
{
	if (_state >= ADAPTER_ACTIVE)
		throw XERROR_FMT(XError, "Adapter(%s) already activated", _name.c_str());

	_state = ADAPTER_ACTIVE;
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		_listeners[i]->activate();
	}
}

void StAdapter::deactivate()
{
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

void StAdapter::wait()
{
	// TODO
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		_listeners[i]->deactivate();
	}
}

std::vector<std::string> StAdapter::getServices() const
{
	std::vector<std::string> rs;

	for (std::map<std::string, ServantPtr>::const_iterator iter = _srvMap.begin(); iter != _srvMap.end(); ++iter)
	{
		rs.push_back(iter->first);
	}
	return rs;
}


ProxyPtr StAdapter::addServant(const std::string& service, Servant* servant)
{
	ProxyPtr prx;

	if (servant && !service.empty())
	{
		if (_state >= ADAPTER_FINISHED)
			throw XERROR_FMT(XError, "Adapter(%s) already deactivated", _name.c_str());
		_srvMap[service] = ServantPtr(servant);
		_hint = _srvMap.begin();
		prx = _engine->stringToProxy(service + ' ' + _endpoints);
	}
	return prx;
}

ServantPtr StAdapter::findServant(const std::string& service) const
{
	ServantPtr srv;

	if (service.length() == 1 && service.data()[0] == 0)
	{
		srv = _engine;
	}
	else
	{
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

ServantPtr StAdapter::removeServant(const std::string& service)
{
	ServantPtr srv;

	std::map<std::string, ServantPtr>::iterator iter = _srvMap.find(service);
	if (iter != _srvMap.end())
	{
		srv = iter->second;
		_srvMap.erase(service);
		_hint = _srvMap.begin();
	}
	return srv;
}

ContextPtr StProxy::getContext() const
{
	return _ctx;
}

void StProxy::setContext(const ContextPtr& ctx)
{
	_ctx = ctx && !ctx->empty() ? ctx: ContextPtr();
}

ConnectionPtr StProxy::getConnection() const
{
	ConnectionPtr con;
	if (_cons.size())
	{
		con = _cons[_idx];
	}
	return con;
}

void StProxy::resetConnection()
{
	for (size_t i = 0; i < _cons.size(); ++i)
		_cons[i].reset();
	_idx = 0;
}

ResultPtr StProxy::emitQuest(const QuestPtr& quest, const CompletionPtr& completion)
{
	StResultPtr r;
	bool twoway = quest->txid();
	if (!twoway && completion)
		throw XERROR_MSG(XLogicError, "completion callback set for oneway msg");

	xstr_t xs = XSTR_CXX(_service);
	if (xs.len && !xstr_equal(&quest->service(), &xs))
	{
		quest->setService(xs);
	}

	if (twoway)
		r.reset(new StResult(this, quest, completion));

	try {
		ConnectionIPtr con;
		if (_incoming)
			con = _cons[0];
		else
			con = pickConnection(quest);

		con->sendQuest(quest, r);
	}
	catch (XError& ex)
	{
		r->giveError(ex);
	}
	return r;
}

bool StProxy::retryQuest(const ResultIPtr& r)
{
	const QuestPtr& quest = r->quest();
	ConnectionIPtr con;
	try {
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

void StProxy::onConnectionError(const ConnectionIPtr& con, const QuestPtr& quest)
{
	// TODO: emit the quest again using another connection.
}


static int64_t usec_diff(const struct timeval* t1, const struct timeval* t2)
{
	int64_t x1 = t1->tv_sec * 1000000 + t1->tv_usec;
	int64_t x2 = t2->tv_sec * 1000000 + t2->tv_usec;
	return x1 - x2;
}

struct StEngine::StThrob: public STimerTask
{
	char _start_time[24];
	int _euid;
	char _euser[32];
	std::string _id;
	std::string _listen;
	std::string _logword;
	bool _enable;
	bool _debut;
	int _minute;
	struct timeval _utv;
	struct rusage _usage;

	StThrob(time_t start, const std::string& id)
		: _euid(-1), _id(id), _enable(true), _debut(true), _minute(0)
	{
		dlog_local_time_str(_start_time, start, true);
		gettimeofday(&_utv, NULL);
		getrusage(RUSAGE_SELF, &_usage);

		_euser[0] = 0;
		_logword = engine_version_rcsid;
	}

	virtual ~StThrob()
	{
	}

	void enable_log(bool b)
	{
		_enable = b;
	}

	void set_logword(const std::string& logword)
	{
		_logword = logword;
	}

	void set_listen(const std::string& listenAddress)
	{
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
		return _logword;
	}

	virtual void runTimerTask(const STimerPtr& timer)
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

				char shadow[48];
				if (xic_passport_shadow)
					snprintf(shadow, sizeof(shadow), "%zd,cipher:%s", xic_passport_shadow->count(),
						MyCipher::get_cipher_name_from_id(xic_cipher));
				else
					strcpy(shadow, "-");

				const char *tag = _debut ? "DEBUT" : "THROB";
				_debut = false;
				xdlog(NULL, NULL, tag, ST_ENGINE_VERSION,
					"id=%s start=%s info=euser:%s,MHz:%.0f,cpu:%.1f%%,xlog:%d,shadow:%s listen=%s %s",
					_id.c_str(), _start_time,
					_euser, (freq / 1000000.0), cpu, xlog_level, shadow,
					_listen.c_str(), _logword.c_str());
			}
		}

		timer->addTask(this, timeout);
	}
};

StEngine::StEngine(const SettingPtr& setting, const std::string& name)
	: EngineI(setting, name)
{
	if (name == "xic")
		throw XERROR_MSG(XError, "Engine name can't be \"xic\"");

	_stopped = false;
	_runCond = st_cond_new();
	_timer = STimer::create();
	_srvPool.reset(new StThreadPool());
	_cliPool.reset(new StThreadPool());

	if (_setting)
	{ 
		ssize_t thr_max = 0, stack_size = 0;

		if (!_name.empty())
		{
			thr_max = _setting->getInt(_name + ".SThreadPool.Server.SizeMax");
			stack_size = _setting->getInt(_name + ".SThreadPool.Server.StackSize");
		}

		if (thr_max <= 0)
			thr_max = _setting->getInt("xic.SThreadPool.Server.SizeMax");
		if (stack_size <= 0)
			stack_size = _setting->getInt("xic.SThreadPool.Server.StackSize");

		if (thr_max > 0)
			_srvPool->_thrMax = thr_max;
		if (stack_size > 0)
			_srvPool->_stackSize = stack_size;


		thr_max = 0;
		stack_size = 0;
		if (!_name.empty())
		{
			thr_max = _setting->getInt(_name + ".SThreadPool.Client.SizeMax");
			stack_size = _setting->getInt(_name + ".SThreadPool.Client.StackSize");
		}

		if (thr_max <= 0)
			thr_max = _setting->getInt("xic.SThreadPool.Client.SizeMax");
		if (stack_size <= 0)
			stack_size = _setting->getInt("xic.SThreadPool.Client.StackSize");

		if (thr_max > 0)
			_cliPool->_thrMax = thr_max;
		if (stack_size > 0)
			_cliPool->_stackSize = stack_size;
	}

	xref_inc();
	_throb.reset(new StThrob(::time(NULL), _id));
	_throb->runTimerTask(_timer);
	_timer->addTask(this, CON_REAP_INTERVAL);
	_timer->start();
	xref_dec_only();
}

StEngine::~StEngine()
{
	st_cond_destroy(_runCond);
}

void StEngine::setSecretBox(const SecretBoxPtr& secretBox)
{
	xic_passport_secret = secretBox;
}

void StEngine::setShadowBox(const ShadowBoxPtr& shadowBox)
{
	xic_passport_shadow = shadowBox;
}

SecretBoxPtr StEngine::getSecretBox()
{
	return xic_passport_secret;
}

ShadowBoxPtr StEngine::getShadowBox()
{
	return xic_passport_shadow;
}

void StEngine::throb(const std::string& logword)
{
	_throb->set_logword(logword);
	_throb->enable_log(true);
}

void StEngine::runTimerTask(const STimerPtr& timer)
{
	time_t before;
	time_t now = this->time();
	bool shadow_changed = false;

	if (xic_passport_secret)
	{
		SecretBoxPtr sb = xic_passport_secret->reload();
		if (sb)
		{
			xic_passport_secret.swap(sb);
		}
	}

	if (xic_passport_shadow)
	{
		ShadowBoxPtr sb = xic_passport_shadow->reload();
		if (sb)
		{
			shadow_changed = true;
			xic_passport_shadow.swap(sb);
		}
	}

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
	}

	timer->addTask(this, CON_REAP_INTERVAL);
}

ProxyPtr StEngine::_makeFixedProxy(const std::string& service, StConnection *con)
{
	if (_stopped)
		throw XERROR(EngineStoppedException);

	StProxyPtr prx;
	ProxyMap::iterator iter = _proxyMap.find(service);
	if (iter != _proxyMap.end())
	{
		prx = iter->second;
		if (prx->getConnection().get() == con)
			return prx;

		_proxyMap.erase(iter);
	}

	prx.reset(new StProxy(service, this, con));
	_proxyMap[service] = prx;
	return prx;
}

ProxyPtr StEngine::stringToProxy(const std::string& proxy)
{
	if (_stopped)
		throw XERROR(EngineStoppedException);

	StProxyPtr prx;
	ProxyMap::iterator iter = _proxyMap.find(proxy);
	if (iter == _proxyMap.end())
	{
		prx.reset(new StProxy(proxy, this, NULL));
		if (prx)
			_proxyMap[proxy] = prx;
	}
	else
	{
		prx = iter->second;
	}
	return prx;
}

void StEngine::incomingConnection(const StConnectionPtr& con)
{
	_incomingCons.push_back(con);
}

ConnectionIPtr StEngine::makeConnection(const std::string& service, const std::string& endpoint)
{
	if (_stopped)
		throw XERROR(EngineStoppedException);

	int attempt = 0;
	ConnectionMap::iterator iter = _conMap.find(endpoint);
	if (iter != _conMap.end())
	{
 		if (isLive(iter->second->state()))
			return iter->second;
		attempt = iter->second->attempt() + 1;
	}

	StConnectionPtr con(new StConnection(this, service, endpoint, attempt));
	if (con)
	{
		_conMap[endpoint] = con;
	}
	return con;
}


AdapterPtr StEngine::createAdapter(const std::string& adapterName, const std::string& endpoints)
{
	const std::string& name = adapterName.empty() ? "xic" : adapterName;

	if (_stopped)
		throw XERROR(EngineStoppedException);

	std::string eps = endpoints;
	if (eps.empty() && _setting)
		eps = _setting->wantString(name + ".Endpoints");

	if (eps.empty())
		throw XERROR_FMT(XError, "No endpoints for Adapter(%s)", name.c_str());

	StAdapterPtr adapter(new StAdapter(this, name, eps));
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

AdapterPtr StEngine::createSlackAdapter()
{
	if (_stopped)
		throw XERROR(EngineStoppedException);

	StAdapterPtr adapter(new StAdapter(this, "", ""));
	if (adapter)
	{
		if (!_adapterMap.insert(std::make_pair(adapter->name(), adapter)).second)
			throw XERROR_FMT(XError, "Adapter(%s) already created", adapter->name().c_str());
	}
	return adapter;
}

time_t StEngine::time()
{
	return st_time();
}

int StEngine::sleep(int seconds)
{
	return st_sleep(seconds);
}

static void *wait_and_exit(void *arg)
{
	int seconds = (intptr_t)arg;
	st_sleep(seconds);
	exit(1);
}

void StEngine::_doom(int seconds)
{
	st_thread_t thr = st_thread_create(wait_and_exit, (void *)(intptr_t)seconds, 0, 0);
	if (!thr)
	{
		exit(3);
	}
}

void StEngine::waitForShutdown()
{
	xic::readyToServe(_setting);

	if (!_stopped)
		st_cond_wait(_runCond);

	_timer->waitForCancel();

	_proxyMap.clear();
}

int StEngine::check_stop()
{
	if (_stopped)
	{
		int numThr = _srvPool->_thrNum + _cliPool->_thrNum;
		if (numThr > 0)
			return 919;	// random less than 1000 milliseconds

		st_cond_broadcast(_runCond);
		_timer->cancel();
		return 0;
	}
	return 0;
}

void StEngine::shutdown()
{
	if (_stopped)
		return;
	_stopped = true;

	AdapterMap adapterMap;
	ConnectionList incomingCons;
	ConnectionMap conMap;

	_adapterMap.swap(adapterMap);
	_incomingCons.swap(incomingCons);
	_conMap.swap(conMap);

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
		_timer->addTask(STimerTask::create(this, &StEngine::check_stop), msec);

	// wait at most SHUTDOWN_WAIT seconds
	_doom(SHUTDOWN_WAIT);
}


StThreadPool::StThreadPool()
{
	_thrNum = 0;
	_thrMax = DEFAULT_THR_MAX;
	_stackSize = DEFAULT_STACK_SIZE;
	_thrCond = st_cond_new();
}

StThreadPool::~StThreadPool()
{
	st_cond_destroy(_thrCond);
}

void StThreadPool::wait_thread()
{
	if (_thrNum >= _thrMax)
	{
		if (xic_dlog_warn)
			dlog("XIC.WARN", "#=SThreadPool.SizeMax(%zd) limits reached, size=%zd", _thrMax, _thrNum);

		do
		{
			st_cond_wait(_thrCond);
		} while (_thrNum >= _thrMax);
	}
	++_thrNum;
}

void StThreadPool::signal_thread()
{
	--_thrNum;
	if (_thrNum < _thrMax)
		st_cond_signal(_thrCond);
}

template<class T>
struct ThreadEnv
{
	static st_thread_t create(T* obj, void (T::*mf)(), const StThreadPoolPtr& pool)
	{
		UniquePtr<ThreadEnv> p(new ThreadEnv(obj, mf, pool));
		pool->wait_thread();
		st_thread_t thr = st_thread_create(routine, p.get(), 0, pool->_stackSize);
		if (!thr)
			pool->signal_thread();
		else
			p.release();
		return thr;
	}

private:
	static void* routine(void *arg)
	{
		ThreadEnv* p = (ThreadEnv*)arg;

		XPtr<T> obj;
		StThreadPoolPtr pool;
		obj.swap(p->_obj);
		pool.swap(p->_pool);
		void (T::*mf)() = p->_mf;
		delete p;

		(obj.get()->*mf)();
		pool->signal_thread();
		return NULL;
	}

	ThreadEnv(T* obj, void (T::*mf)(), const StThreadPoolPtr& pool)
		: _obj(obj), _mf(mf), _pool(pool)
	{
	}

private:
	XPtr<T> _obj;
	void (T::*_mf)();
	StThreadPoolPtr _pool;
};

template<class T>
st_thread_t StEngine::create_server_thread(T* obj, void (T::*mf)())
{
	st_thread_t thr = ThreadEnv<T>::create(obj, mf, _srvPool);
	return thr;
}

template<class T>
st_thread_t StEngine::create_client_thread(T* obj, void (T::*mf)())
{
	st_thread_t thr = ThreadEnv<T>::create(obj, mf, _cliPool);
	return thr;
}

void StEngine::_info(AnswerWriter& aw)
{
	aw.param("engine.start_time", _throb->start_time());
	aw.param("engine.type", "StEngine");
	aw.param("engine.version", ST_ENGINE_VERSION);
	aw.param("throb.enabled", _throb->enabled());
	aw.param("throb.logword", _throb->logword());

	aw.param("threadpool.server.current", _srvPool->_thrNum);
	aw.param("threadpool.server.size", 0);
	aw.param("threadpool.server.sizemax", _srvPool->_thrMax);
	aw.param("threadpool.server.stacksize", _srvPool->_stackSize);

	aw.param("threadpool.client.current", _cliPool->_thrNum);
	aw.param("threadpool.client.size", 0);
	aw.param("threadpool.client.sizemax", _cliPool->_thrMax);
	aw.param("threadpool.client.stacksize", _cliPool->_stackSize);

	aw.param("adapter.count", _adapterMap.size());
	aw.param("proxy.count", _proxyMap.size());
	aw.param("connection.count", _conMap.size());

	VListWriter lw;

	lw = aw.paramVList("adapters");
	for (AdapterMap::iterator iter = _adapterMap.begin(); iter != _adapterMap.end(); ++iter)
	{
		StAdapterPtr& adapter = iter->second;

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


static int sig_fds[2];

void sig_catcher(int sig)
{
	int err = errno;
	write(sig_fds[1], &sig, sizeof(int));
	errno = err;
}

static void *sig_sthread(void *arg)
{
	st_netfd_t nfd = st_netfd_open(sig_fds[0]);

	while (true)
	{
		int sig;
		int rc = st_read(nfd, &sig, sizeof(int), ST_UTIME_NO_TIMEOUT);
		if (rc != sizeof(int))
			throw XERROR_FMT(XError, "st_read()=%d", rc);

		if (xic_dlog_warn)
			dlog("XIC.WARN", "#=signal(%d) catched", sig);

		if (xic_engine)
			xic_engine->shutdown();
		else
			exit(1);
	}
}

static bool _st_initialize_once()
{
	static bool _inited;

	if (!_inited)
	{
		int r;
		st_set_eventsys(ST_EVENTSYS_ALT);
		r = st_init();
		if (r == 0)
			_inited = true;
	}
	return _inited;
}

EnginePtr Engine::st(const SettingPtr& setting, const std::string& name)
{
	if (xic_engine)
		throw XERROR_MSG(XLogicError, "Only one instance of xic::Engine is allowed in a process");

	_st_initialize_once();
	st_timecache_set(1);

	if (pipe(sig_fds) != 0)
		throw XERROR_FMT(XError, "pipe() failed, errno=%d", errno);

	struct sigaction sigact;
	sigact.sa_handler = sig_catcher;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	prepareEngine(setting);
	xic_engine.reset(new StEngine(setting, name));

	if (st_thread_create(sig_sthread, NULL, 0, 0) == NULL)
		throw XERROR_FMT(XError, "st_thread_create() failed, errno=%d", errno);

	return xic_engine;
}


class StApp: public ApplicationI
{
	xic_application_function _func;
public:
	StApp(xic_application_function func)
		: ApplicationI(xic::Engine::st), _func(func)
	{
	}

	int run(int argc, char **argv)
	{
		return _func(argc, argv, engine());
	}
};

int xic::start_xic_st(xic_application_function func, int argc, char **argv, const SettingPtr& setting)
{
	xlog_level = XLOG_WARN;
	StApp app(func);
	return app.main(argc, argv, setting);
}

