#include "Connection.h"
#include "xic_Engine.h"
#include "xslib/xnet.h"
#include "xslib/xbuf.h"
#include "xslib/ostk.h"
#include "xslib/vbs_pack.h"
#include "xslib/ScopeGuard.h"
#include "xic/VData.h"
#include <errno.h>
#include <limits.h>
#include <unistd.h>


namespace xic
{

XE_(XError, IOError);
XE_(XError, ProtocolError);

struct Header {
	uint8_t magic;
	uint8_t version;
	uint8_t msgType;
	uint8_t flags;
	uint32_t bodySize;

	bool isCheck() const
	{
		return (memcmp(this, "XIC\0", 4) == 0);
	}

	bool isHello() const
	{
		return (memcmp(this, "XIH\0\0\0\0\0", 8) == 0);
	}
};

static struct Header default_hdr = { 'X', 'I', 0, 0, 0 };
static struct Header bye_hdr = { 'X','I','B', 0, 0 };


Connection::Connection(const EnginePtr& engine, const std::string& endpoint)
	: _engine(engine), _last_id(0), _timeout(-1), _priority(0), _fd(-1), _state(ST_INIT)
{
	xstr_t xs = XSTR_CXX(endpoint);
	xstr_t netloc, proto, host;
	xstr_token_cstr(&xs, " \t", &netloc);
	_endpoint = make_string(netloc);

	xstr_delimit_char(&netloc, '+', &proto);
	xstr_delimit_char(&netloc, '+', &host);
	_port = xstr_to_long(&netloc, NULL, 10);
	_peer_ip[0] = 0;

	if (proto.len && !xstr_case_equal_cstr(&proto, "tcp"))
		throw XERROR_FMT(XError, "Unsupported transport protocol '%.*s'", XSTR_P(&proto));

	_proto = make_string(proto);
	_host = make_string(host);

	if (host.len == 0)
		_priority += 30;
	else if (xstr_case_equal_cstr(&host, "localhost"))
		_priority += 30;
	else if (xstr_find_char(&host, 0, ':') >= 0 && host.len < 40)
	{
		char buf[40];
		xstr_copy_to(&host, buf, sizeof(buf));
		uint8_t ipv6[16];
		if (xnet_ipv6_aton(buf, ipv6))
		{
			if (xnet_ipv6_is_loopback(ipv6))
				_priority += 30;
			else if (xnet_ipv6_is_uniquelocal(ipv6))
				_priority += 20;
			else if (xnet_ipv6_is_ipv4(ipv6))
			{
				uint32_t ipv4 = ntohl(*(uint32_t*)&ipv6[12]);
				if (xnet_ipv4_is_loopback(ipv4))
					_priority += 30;
				else if (xnet_ipv4_is_internal(ipv4))
					_priority += 20;
				else	
					_priority += 10;
			}
			else
				_priority += 10;
		}
	}
	else if (xstr_char_in_bset(&host, 0, &digit_bset) && host.len < 16)
	{
		char buf[16];
		xstr_copy_to(&host, buf, sizeof(buf));
		uint32_t ip;
		if (xnet_ipv4_aton(buf, &ip))
		{
			if (xnet_ipv4_is_loopback(ip))
				_priority += 30;
			else if (xnet_ipv4_is_internal(ip))
				_priority += 20;
			else 
				_priority += 10;
		}
	}

	xstr_t value;
	while (xstr_token_cstr(&xs, " \t", &value))
	{
		xstr_t key;
		xstr_delimit_char(&value, '=', &key);
		if (xstr_equal_cstr(&key, "timeout"))
		{
			_timeout = xstr_to_long(&value, NULL, 10);
			_endpoint += " timeout=" + value;
		}
	}
}

Connection::~Connection()
{
	if (_fd >= 0)
	{
		_close();
	}
}

/* Return true if Hello is read 
 */
bool Connection::_read_hello_or_check(int* timeout, ostk_t *ostk, xstr_t *command, vbs_dict_t *dict)
{
	struct Header hdr;
	size_t resid = sizeof(hdr);
	int rc = xnet_read_resid(_fd, &hdr, &resid, timeout);
	if (rc < 0)
		_throw_IOError("reading hello message", rc);

	if (hdr.isHello())
		return true;

	if (!hdr.isCheck())
		throw XERROR_FMT(XError, "Unexpected message '%c' [%#x], endpoint=%s", hdr.msgType, hdr.msgType, _endpoint.c_str());

	xnet_msb32(&hdr.bodySize);
	if (hdr.bodySize > INT_MAX)
		throw XERROR_FMT(ProtocolError, "bodySize(%u) too large", hdr.bodySize);
	
	xstr_t res = XSTR_INIT((uint8_t*)ostk_alloc(ostk, hdr.bodySize), hdr.bodySize);
	if (!res.data)
	{
		throw XERROR_FMT(XError, "ostk_alloc() failed");
	}

	resid = res.len;
	rc = xnet_read_resid(_fd, res.data, &resid, timeout);
	if (rc < 0)
	{
		_throw_IOError("reading check body", rc);
	}

	vbs_unpacker_t uk = VBS_UNPACKER_INIT(res.data, res.len, -1);
	if (vbs_unpack_xstr(&uk, command) < 0)
	{
		throw XERROR_MSG(XError, "failed to decode comand of Check message");
	}

	if (vbs_unpack_dict(&uk, dict, &ostk_xmem, ostk) < 0)
	{
		throw XERROR_MSG(XError, "failed to decode args of Check message");
	}

	if (xstr_equal_cstr(command, "FORBIDDEN"))
	{
		xic::VDict args(dict);
		xstr_t reason = args.getXstr("reason");
		throw XERROR_MSG(xic::AuthenticationException, make_string(reason));
	}

	return false;
}

void Connection::_send_check_message(int *timeout, const xic::CheckPtr& check)
{
	int iov_num;
	struct iovec *iov = check->get_iovec(&iov_num);
	int rc = xnet_writev_resid(_fd, &iov, &iov_num, timeout);
	if (rc < 0)
	{
		_throw_IOError("writing", rc);
	}
}

void Connection::_check(int *timeout)
{
	ostk_t *ostk = ostk_create(0);
	ON_BLOCK_EXIT(ostk_destroy, ostk);

	void *sentry = ostk_alloc(ostk, 0);

	xstr_t command;
	vbs_dict_t dict;
	if (_read_hello_or_check(timeout, ostk, &command, &dict))
		return;

	xic::VDict args(&dict);
	if (xstr_equal_cstr(&command, "AUTHENTICATE"))
	{
		Srp6aClientPtr srp6a;
		xstr_t method = args.getXstr("method");
		if (!xstr_equal_cstr(&method, "SRP6a"))
		{
			throw XERROR_FMT(ProtocolError, "Unknown Authentication method [%.*s]", XSTR_P(&method));
		}
		else
		{
			SecretBoxPtr sb = _engine->getSecretBox();
			if (!sb)
				throw XERROR_MSG(XError, "No secret infos supplied to the engine");

			xstr_t identity, password;
			std::string proto = !_proto.empty() ? _proto : "tcp";
			std::string host = !_host.empty() ? _host : _peer_ip;
			if (!sb->find(_service, proto, host, _port, identity, password))
				throw XERROR_FMT(XError, "No matched secret for %s@%s+%s+%d", _service.c_str(), proto.c_str(), host.c_str(), _port);

			srp6a = new Srp6aClient();
			srp6a->set_identity(identity, password);

			xic::CheckWriter cw("SRP6a1");
			cw.param("I", identity);
			_send_check_message(timeout, cw);
		}

		ostk_free(ostk, sentry);
		if (_read_hello_or_check(timeout, ostk, &command, &dict))
			return;

		if (!xstr_equal_cstr(&command, "SRP6a2"))
		{
			throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&command));
		}
		else
		{
			xic::VDict args(&dict);
			xstr_t hash = args.getXstr("hash");
			xstr_t N = args.wantBlob("N");
			xstr_t g = args.wantBlob("g");
			xstr_t s = args.wantBlob("s");
			xstr_t B = args.wantBlob("B");

			srp6a->set_hash(hash);
			srp6a->set_parameters(g, N, N.len * 8);
			srp6a->set_salt(s);
			srp6a->set_B(B);
			xstr_t A = srp6a->gen_A();
			xstr_t M1 = srp6a->compute_M1();

			xic::CheckWriter cw("SRP6a3");
			cw.paramBlob("A", A);
			cw.paramBlob("M1", M1);
			_send_check_message(timeout, cw);
		}

		ostk_free(ostk, sentry);
		if (_read_hello_or_check(timeout, ostk, &command, &dict))
			return;

		if (!xstr_equal_cstr(&command, "SRP6a4"))
		{
			throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&command));
		}
		else
		{
			xic::VDict args(&dict);
			xstr_t M2_srv = args.wantBlob("M2");
			xstr_t M2_cli = srp6a->compute_M2();
			if (!xstr_equal(&M2_srv, &M2_cli))
				throw XERROR_MSG(XError, "Server is fake? srp6a M2 not equal");
		}

		ostk_free(ostk, sentry);
		if (_read_hello_or_check(timeout, ostk, &command, &dict))
			return;

		throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&command));
	}
	else
	{
		throw XERROR_FMT(XError, "Unknown command of Check message [%.*s]", XSTR_P(&command));
	}
}

int Connection::_connect(int *timeout)
{
	if (_fd < 0)
	{
		_fd = xnet_tcp_connect_nonblock(_host.c_str(), _port);

		if (_fd >= 0)
		{
			xnet_get_peer_ip_port(_fd, _peer_ip);
			xnet_set_tcp_nodelay(_fd);
			xnet_set_keepalive(_fd);
			xnet_set_linger_on(_fd, 0);
			_state = ST_WAITING_HELLO;

			_check(timeout);

			_state = ST_ACTIVE;
		}
	}
	return _fd;
}

void Connection::close()
{
	_close();
}

void Connection::_close()
{
	if (_fd >= 0)
	{
		if (_state < ST_CLOSING)
		{
			_state = ST_CLOSE;
			size_t resid = sizeof(bye_hdr);
			int timeout = 3000;
			xnet_write_resid(_fd, &bye_hdr, &resid, &timeout);
			_state = ST_CLOSING;
		}

		::close(_fd);
		_fd = -1;
		_state = ST_CLOSED;
	}
}

void Connection::_throw_IOError(const char *rdwr, int rc)
{
	::close(_fd);
	_fd = -1;

	if (rc == -3)
		throw XERROR_FMT(IOError, "IO timeout on %s, endpoint=%s", rdwr, _endpoint.c_str());

	if (rc == -2)
		throw XERROR_FMT(IOError, "Socket closed on %s, endpoint=%s", rdwr, _endpoint.c_str());

	throw XERROR_FMT(IOError, "IO error on %s, errno=%d, endpoint=%s", rdwr, errno, _endpoint.c_str());
}

void Connection::_sendMessage(int64_t id, const xstr_t& service, const xstr_t& method,
				const rope_t& args, const std::string& ctx, int *timeout)
{
	if (_fd < 0)
	{
		if (_connect(timeout) < 0)
		{
			throw XERROR_FMT(XError, "connect() failed, endpoint=%s", _endpoint.c_str());
		}
	}

	unsigned char buf[512];
	xbuf_t xb = XBUF_INIT(buf, sizeof(buf));
	struct Header *hdr = (struct Header *)xb.data;
	*hdr = default_hdr;
	xb.len += sizeof(struct Header);
	vbs_packer_t pk = VBS_PACKER_INIT(xbuf_xio.write, &xb, 100);
	vbs_pack_integer(&pk, id);
	vbs_pack_xstr(&pk, &service);
	vbs_pack_xstr(&pk, &method);
	if (pk.error)
		throw XERROR_MSG(XError, "vbs_packer_t error, service or method too long?");

	int has_ctx = ctx.empty() ? 0 : 1;
	int count = 1 + has_ctx + args.block_count;
	struct iovec *iov = (struct iovec *)alloca(count * sizeof(iov[0]));
	iov[0].iov_base = xb.data;
	iov[0].iov_len = xb.len;
	if (has_ctx)
	{
		iov[1].iov_base = (char *)ctx.data();
		iov[1].iov_len = ctx.length();
	}
	else
	{
		iov[1].iov_base = vbs_packed_empty_dict.data;
		iov[1].iov_len = vbs_packed_empty_dict.len;
	}
	rope_iovec(&args, &iov[2]);
	hdr->msgType = 'Q';
	hdr->bodySize = xb.len - sizeof(struct Header) + ctx.length() + args.length;
	xnet_msb32(&hdr->bodySize);

	int rc = xnet_writev_resid(_fd, &iov, &count, timeout);
	if (rc < 0)
	{
		_throw_IOError("writing", rc);
	}
}

xstr_t Connection::invoke(const xstr_t& service, const xstr_t& method, const rope_t& args, const std::string& ctx)
try
{
	int timeout = _timeout;
	int64_t id = ++_last_id;

	_sendMessage(id, service, method, args, ctx, &timeout);

	struct Header hdr;
	size_t resid = sizeof(hdr);
	int rc = xnet_read_resid(_fd, &hdr, &resid, &timeout);
	if (rc < 0)
		_throw_IOError("reading answer header", rc);

	if (hdr.magic != 'X'
		|| hdr.version != 'I'
		|| hdr.flags != 0)
	{
		throw XERROR_FMT(ProtocolError, "Protocol header error, endpoint=%s", _endpoint.c_str());
	}

	xnet_msb32(&hdr.bodySize);
	if (hdr.bodySize > INT_MAX)
		throw XERROR_FMT(ProtocolError, "bodySize(%u) too large", hdr.bodySize);

	if (hdr.msgType == 'B')
	{
		if (hdr.bodySize != 0)
			throw XERROR_MSG(ProtocolError, "Invalid close message");

		if (_fd >= 0)
		{
			::close(_fd);
			_fd = -1;
			_state = ST_CLOSED;
		}
		throw XERROR_MSG(XError, "peer closed");
	}
	else if (hdr.msgType != 'A')
		throw XERROR_FMT(ProtocolError, "Unexpected message type(%#x), endpoint=%s", hdr.msgType, _endpoint.c_str());

	xstr_t res = XSTR_INIT((unsigned char*)malloc(hdr.bodySize), hdr.bodySize);
	if (!res.data)
	{
		throw XERROR_FMT(XError, "malloc() failed");
	}
	ScopeGuard guard = MakeGuard(free, res.data);

	resid = res.len;
	rc = xnet_read_resid(_fd, res.data, &resid, &timeout);
	if (rc < 0)
	{
		_throw_IOError("reading answer body", rc);
	}

	vbs_unpacker_t uk = VBS_UNPACKER_INIT(res.data, res.len, -1);
	intmax_t answer_id;
	if (vbs_unpack_integer(&uk, &answer_id) < 0 || answer_id != id)
	{
		throw XERROR_FMT(ProtocolError, "Answer txid(%jd) is not equal to quest txid(%jd)", answer_id, (intmax_t)id);
	}

	guard.dismiss();
	return res;
}
catch (XError& ex)
{
	if (_fd >= 0)
	{
		::close(_fd);
		_fd = -1;
	}
	throw;
}

void Connection::invoke_oneway(const xstr_t& service, const xstr_t& method, const rope_t& args, const std::string& ctx)
try
{
	int timeout = _timeout;
	_sendMessage(0, service, method, args, ctx, &timeout);
}
catch (XError& ex)
{
	if (_fd >= 0)
	{
		::close(_fd);
		_fd = -1;
	}
	throw;
}



}

