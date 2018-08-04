#include "Connection.h"
#include "xic_Engine.h"
#include "xslib/xnet.h"
#include "xslib/xbuf.h"
#include "xslib/ostk.h"
#include "xslib/vbs_pack.h"
#include "xslib/ScopeGuard.h"
#include "xic/VData.h"
#include "dlog/dlog.h"
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
		return (memcmp(this, "X!C\0", 4) == 0);
	}

	bool isHello() const
	{
		return (memcmp(this, "X!H\0\0\0\0\0", 8) == 0);
	}
};

static struct Header default_hdr = { 'X', '!', 0, 0, 0 };
static struct Header bye_hdr = { 'X','!','B', 0, 0 };


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
		xstr_copy_cstr(&host, buf, sizeof(buf));
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
		xstr_copy_cstr(&host, buf, sizeof(buf));
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
	struct iovec *body_iov = check->body_iovec(&iov_num);

	ostk_t *ostk = check->ostk();
	struct iovec *iov = OSTK_ALLOC(ostk, struct iovec, iov_num + 2);
	memcpy(&iov[1],	body_iov, iov_num * sizeof(struct iovec));

	XicMessage::Header *hdr = OSTK_ALLOC_ONE(ostk, XicMessage::Header);
	hdr->magic = 'X';
	hdr->version = '!';
	hdr->msgType = 'C';
	hdr->flags = 0;
	hdr->bodySize = xnet_m32(check->bodySize());

	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(XicMessage::Header);
	++iov_num;

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

			xstr_t cipher = args.getXstr("CIPHER");
			MyCipher::CipherSuite suite = (cipher.len == 0) ? MyCipher::CLEARTEXT
					: MyCipher::get_cipher_id_from_name(make_string(cipher));
			if (suite < 0)
				throw XERROR_FMT(XError, "Unknown CIPHER \"%.*s\"", XSTR_P(&cipher));

			if (suite > 0)
			{
				xstr_t K = srp6a->compute_K();
				_cipher = new MyCipher(suite, K.data, K.len, false);
				int mode = args.getInt("MODE", 0);
				if (mode == 0)
					_cipher->setMode0(true);
			}
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

/* NB: the content of args may be modified after the function return.
 */
void Connection::_sendMessage(int64_t id, const xstr_t& service, const xstr_t& method,
				const rope_t& args, const std::string& quest_ctx, int *timeout)
{
	if (_fd < 0)
	{
		if (_connect(timeout) < 0)
		{
			throw XERROR_FMT(XError, "connect() failed, endpoint=%s", _endpoint.c_str());
		}
	}

	xstr_t ctx = XSTR_CXX(quest_ctx);
	if (!ctx.len)
		ctx = vbs_packed_empty_dict;

	struct Header hdr = default_hdr;

	int buf_len = 30 + service.len + method.len + ctx.len;
	uint8_t *buf = NULL;
	uint8_t *ptr4free = NULL;
	if (buf_len > 4096)
	{
		buf = (uint8_t *)malloc(buf_len);
		ptr4free = buf;
	}
	else
	{
		buf = (uint8_t *)alloca(buf_len);
	}
	ON_BLOCK_EXIT(free, ptr4free);

	xbuf_t xb = XBUF_INIT(buf, buf_len);
	vbs_packer_t pk = VBS_PACKER_INIT(xbuf_xio.write, &xb, 100);
	vbs_pack_integer(&pk, id);
	vbs_pack_xstr(&pk, &service);
	vbs_pack_xstr(&pk, &method);
	memcpy(xb.data + xb.len, ctx.data, ctx.len);
	xb.len += ctx.len;

	hdr.msgType = 'Q';
	hdr.bodySize = xb.len + args.length;
	if (_cipher)
	{
		if (_cipher->mode0())
		{
			hdr.flags |= XIC_FLAG_CIPHER_MODE0;
			hdr.bodySize += _cipher->extraSizeMode0();
		}
		else
		{
			hdr.flags |= XIC_FLAG_CIPHER_MODE1;
			hdr.bodySize += _cipher->extraSize();
		}
	}
	xnet_msb32(&hdr.bodySize);

	int count = 2 + args.block_count;
	if (_cipher)
		count += 2;

	struct iovec *iov = (struct iovec *)alloca(count * sizeof(iov[0]));
	int k = 0;

	iov[k].iov_base = &hdr;
	iov[k].iov_len = sizeof(hdr);
	++k;

	if (_cipher && _cipher->mode0())
	{
		iov[k].iov_base = _cipher->oIV;
		iov[k].iov_len = sizeof(_cipher->oIV);
		++k;
	}

	struct iovec *body_iov = &iov[k];
	int body_k = k;

	iov[k].iov_base = xb.data;
	iov[k].iov_len = xb.len;
	++k;

	rope_iovec(&args, &iov[k]);
	k += args.block_count;

	if (_cipher)
	{
		int num = k - body_k;
		if (_cipher->mode0())
		{
			_cipher->oSeqIncreaseMode0();
			_cipher->encryptStartMode0(&hdr, sizeof(hdr));
		}
		else
		{
			_cipher->encryptStart(&hdr, sizeof(hdr));
		}

		for (int i = 0; i < num; ++i)
		{
			_cipher->encryptUpdate(body_iov[i].iov_base, body_iov[i].iov_base, body_iov[i].iov_len);
		}
		_cipher->encryptFinish();

		iov[k].iov_base = _cipher->oMAC;
		iov[k].iov_len = sizeof(_cipher->oMAC);
		++k;
	}
	assert(k == count);

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

	if (hdr.magic != 'X' || hdr.version != '!')
	{
		throw XERROR_FMT(ProtocolError, "Protocol header error, endpoint=%s", _endpoint.c_str());
	}

	if (hdr.flags & ~XIC_FLAG_MASK)
		throw XERROR_FMT(ProtocolException, "%s Unknown packet header flag %#x", _endpoint.c_str(), hdr.flags);

	int flagCipher = (hdr.flags & (XIC_FLAG_CIPHER_MODE0 | XIC_FLAG_CIPHER_MODE1));
	if (flagCipher && !_cipher)
	{
		throw XERROR_FMT(ProtocolException, "%s CIPHER flag set but No CIPHER negociated before", _endpoint.c_str());
	}

	if (flagCipher == (XIC_FLAG_CIPHER_MODE0 | XIC_FLAG_CIPHER_MODE1))
	{
		throw XERROR_FMT(ProtocolException, "%s CIPHER flag mode0 and mode1 both set", _endpoint.c_str());
	}

	uint32_t bodySize = xnet_m32(hdr.bodySize);
	if (bodySize > INT_MAX)
		throw XERROR_FMT(ProtocolError, "bodySize(%u) too large", bodySize);

	if (hdr.msgType == 'B')
	{
		if (bodySize != 0)
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

	if (flagCipher & XIC_FLAG_CIPHER_MODE0)
	{
		if (bodySize <= _cipher->extraSizeMode0())
			throw XERROR_FMT(MessageSizeException, "%s Invalid packet bodySize %lu", 
					_endpoint.c_str(), (unsigned long)bodySize);
		bodySize -= _cipher->extraSizeMode0();

		resid = sizeof(_cipher->iIV);
		rc = xnet_read_resid(_fd, _cipher->iIV, &resid, &timeout);
		if (rc < 0)
		{
			_throw_IOError("reading cipher IV", rc);
		}

		_cipher->iSeqIncreaseMode0();
		if (!_cipher->decryptCheckSequenceMode0())
			throw XERROR_FMT(ProtocolException, "%s Unmatched sequence number in IV", _endpoint.c_str());
	}
	else if (flagCipher & XIC_FLAG_CIPHER_MODE1)
	{
		if (bodySize <= _cipher->extraSize())
			throw XERROR_FMT(MessageSizeException, "%s Invalid packet bodySize %lu", 
					_endpoint.c_str(), (unsigned long)bodySize);
		bodySize -= _cipher->extraSize();
	}

	xstr_t body = XSTR_INIT((unsigned char*)malloc(bodySize), bodySize);
	if (!body.data)
	{
		throw XERROR_FMT(XError, "malloc() failed");
	}
	ScopeGuard guard = MakeGuard(free, body.data);

	resid = body.len;
	rc = xnet_read_resid(_fd, body.data, &resid, &timeout);
	if (rc < 0)
	{
		_throw_IOError("reading answer body", rc);
	}

	if (flagCipher)
	{
		resid = sizeof(_cipher->iMAC);
		rc = xnet_read_resid(_fd, _cipher->iMAC, &resid, &timeout);
		if (rc < 0)
		{
			_throw_IOError("reading cipher MAC", rc);
		}

		if (flagCipher & XIC_FLAG_CIPHER_MODE0)
			_cipher->decryptStartMode0(&hdr, sizeof(hdr));
		else
			_cipher->decryptStart(&hdr, sizeof(hdr));

		_cipher->decryptUpdate(body.data, body.data, body.len);
		bool ok = _cipher->decryptFinish();
		if (!ok)
			throw XERROR_FMT(ProtocolException, "Msg body failed to decrypt, %s", _endpoint.c_str());
	}

	vbs_unpacker_t uk = VBS_UNPACKER_INIT(body.data, body.len, -1);
	intmax_t answer_id;
	if (vbs_unpack_integer(&uk, &answer_id) < 0 || answer_id != id)
	{
		throw XERROR_FMT(ProtocolError, "Answer txid(%jd) is not equal to quest txid(%jd)", answer_id, (intmax_t)id);
	}

	guard.dismiss();
	return body;
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

