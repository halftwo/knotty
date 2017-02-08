#include "SecretBox.h"
#include "xslib/ScopeGuard.h"
#include "xslib/xlog.h"
#include "xslib/unixfs.h"
#include "xslib/bit.h"
#include "xslib/xnet.h"
#include <errno.h>
#include <sstream>

SecretBoxPtr SecretBox::createFromFile(const std::string& filename)
{
	return new SecretBox(filename);
}

SecretBoxPtr SecretBox::createFromContent(const std::string& content)
{
	xstr_t xs = XSTR_CXX(content);
	return new SecretBox(xs);
}

SecretBox::SecretBox(const std::string& filename)
	: _ostk(NULL), _filename(filename)
{
	try {
		_load();
	}
	catch (...)
	{
		ostk_destroy(_ostk);
		throw;
	}
}

SecretBox::SecretBox(const xstr_t& content)
	: _ostk(NULL)
{
	try {
		_mtime = 0;
		_init(content.data, content.len);
	}
	catch (...)
	{
		ostk_destroy(_ostk);
		throw;
	}
}

SecretBox::~SecretBox()
{
	ostk_destroy(_ostk);
}

SecretBoxPtr SecretBox::reload()
{
	SecretBoxPtr sb;
	try {
		struct stat st;
		if (stat(_filename.c_str(), &st) == 0 && st.st_mtime != _mtime)
		{
			sb = new SecretBox(_filename);
			xlog(XLOG_NOTICE, "secret file reloaded, file=%s", _filename.c_str());
		}
	}
	catch (std::exception& ex)
	{
		xlog(XLOG_WARNING, "secret file failed to reload, file=%s, ex=%s", _filename.c_str(), ex.what());
	}
	return sb;
}

void SecretBox::_load()
{
	struct stat st;
	if (stat(_filename.c_str(), &st) < 0)
		throw XERROR_FMT(XError, "stat() failed, file=%s, errno=%d", _filename.c_str(), errno);

	_mtime = st.st_mtime;

	unsigned char *buf = NULL;
	size_t size = 0;
	ssize_t len = unixfs_get_content(_filename.c_str(), &buf, &size);
	ON_BLOCK_EXIT(free, buf);
	if (len < 0)
		throw XERROR_FMT(XError, "Failed to get content of file `%s`, errno=%d", _filename.c_str(), errno);

	_init(buf, len);
}

void SecretBox::_init(uint8_t *content, ssize_t size)
{
	int lineno = 0;
	xstr_t xs = XSTR_INIT(content, size);
	xstr_t line;
	_ostk = ostk_create(0);
	while (xstr_delimit_char(&xs, '\n', &line))
	{
		++lineno;
		xstr_trim(&line);

		if (line.len == 0 || line.data[0] == '#')
			continue;

		xstr_t key, value;
		if (xstr_key_value(&line, '=', &key, &value) <= 0 || key.len == 0 || value.len == 0)
			throw XERROR_FMT(XError, "Invalid syntax in line %d", lineno);
		
		int n;
		xstr_t tmp, end;
		Secret secret;

		xstr_key_value(&key, '@', &secret.service, &tmp);
		xstr_delimit_char(&tmp, '+', &secret.proto);
		xstr_delimit_char(&tmp, '+', &secret.host);
		if (tmp.len == 0)
		{
			secret.port = 0;
		}
		else
		{
			n = xstr_to_long(&tmp, &end, 10);
			if (n < 0 || n > 65535 || end.len)
				throw XERROR_FMT(XError, "Invalid port `%.*s` in line %d", XSTR_P(&tmp), lineno);
			secret.port = n;
		}

		tmp = secret.host;
		xstr_delimit_char(&tmp, '/', &secret.host);

		if (tmp.len == 0)
		{
			secret.prefix = 128;
		}
		else
		{
			n = xstr_to_long(&tmp, &end, 10);
			if (n <= 0 || n > 128 || end.len)
				throw XERROR_FMT(XError, "Invalid prefix `%.*s` in line %d", XSTR_P(&tmp), lineno);
			secret.prefix = n;
		}

		xstr_key_value(&value, ':', &secret.identity, &secret.password);

		if (secret.host.len > 0 && secret.host.len < 40)
		{
			char buf[40];
			xstr_copy_to(&secret.host, buf, sizeof(buf));
			if (!xnet_ip46_aton(buf, secret.ip6))
				secret.prefix = 128;
			else if (xstr_find_char(&secret.host, 0, ':') < 0 && secret.prefix <= 32)
				secret.prefix += 96;
		}

		secret.service = ostk_xstr_dup(_ostk, &secret.service);
		secret.proto = ostk_xstr_dup(_ostk, &secret.proto);
		secret.host = ostk_xstr_dup(_ostk, &secret.host);
		secret.identity = ostk_xstr_dup(_ostk, &secret.identity);
		secret.password = ostk_xstr_dup(_ostk, &secret.password);
		_sv.push_back(secret);
	}
}

void SecretBox::dump(xio_write_function write, void *cookie)
{
	uint8_t tmpbuf[4096];
	xio_t myio = { NULL, write, NULL, NULL };
	iobuf_t ob = IOBUF_INIT(&myio, cookie, tmpbuf, sizeof(tmpbuf));

	for (SecretVector::iterator iter = _sv.begin(); iter != _sv.end(); ++iter)
	{
		const Secret& s = *iter;

		iobuf_printf(&ob, "%.*s@%.*s+%.*s", XSTR_P(&s.service), XSTR_P(&s.proto), XSTR_P(&s.host));

		if (s.prefix != 128)
		{
			int prefix = (xstr_find_char(&s.host, 0, ':') < 0) ? s.prefix - 96 : s.prefix;
			iobuf_printf(&ob, "/%d", prefix);
		}

		if (s.port)
			iobuf_printf(&ob, "+%d", s.port);
		else
			iobuf_putc(&ob, '+');

		iobuf_printf(&ob, " = %.*s:%.*s\n", XSTR_P(&s.identity), XSTR_P(&s.password));
	}

	iobuf_finish(&ob);
}

std::string SecretBox::getContent()
{
	std::stringstream ss;
	dump(ostream_xio.write, (std::ostream*)&ss);
	return ss.str();
}

bool SecretBox::Secret::match_host(const xstr_t& host, const uint8_t ip6[]) const
{
	if (host.len && xstr_equal(&this->host, &host))
		return true;

	return bitmap_msb_equal(this->ip6, ip6, this->prefix);
}

bool SecretBox::getItem(size_t index, xstr_t& service, xstr_t& proto, xstr_t& host, int& port, xstr_t& identity, xstr_t& password)
{
	if (index >= _sv.size())
		return false;

	const Secret& s = _sv[index];
	service = s.service;
	proto = s.proto;
	host = s.host;
	port = s.port;
	identity = s.identity;
	password = s.password;
	return true;
}

bool SecretBox::find(const std::string& service, const std::string& proto, const std::string& host, int port, xstr_t& identity, xstr_t& password)
{
	xstr_t s = XSTR_CXX(service);
	xstr_t p = XSTR_CXX(proto);
	xstr_t h = XSTR_CXX(host);
	return find(s, p, h, port, identity, password);
}

bool SecretBox::find(const xstr_t& service, const xstr_t& protocol, const xstr_t& hostOrIp, int port, xstr_t& identity, xstr_t& password)
{
	static const xstr_t default_proto = XSTR_CONST("tcp");
	uint8_t ip6[16] = { 0 };
	xstr_t host = hostOrIp;
	xstr_t proto = protocol.len ? protocol : default_proto;

	if (host.len < 40)
	{
		char buf[40];
		xstr_copy_to(&host, buf, sizeof(buf));
		if (xnet_ip46_aton(buf, ip6))
		{
			host.len = 0;
		}
	}

	for (SecretVector::iterator iter = _sv.begin(); iter != _sv.end(); ++iter)
	{
		const Secret& s = *iter;
		if (s.service.len && !xstr_equal(&s.service, &service))
			continue;

		if (s.proto.len && !xstr_equal(&s.proto, &proto))
			continue;

		if (s.host.len && !s.match_host(host, ip6))
			continue;

		if (s.port && s.port != port)
			continue;

		identity = s.identity;
		password = s.password;
		return true;
	}
	return false;
}

#ifdef TEST_SECRET

#include <stdio.h>

int main(int argc, char **argv)
{
	try {
		if (argc < 2)
		{
			fprintf(stderr, "Usage: %s file\n", argv[0]);
			exit(1);
		}

		SecretBoxPtr sb = SecretBox::createFromFile(argv[1]);
		sb->dump(stdio_xio.write, stdout);
		xstr_t identity, password;
		if (!sb->find("Test", "", "::1", 3030, identity, password))
			throw XERROR_MSG(XError, "No matched secret");

		printf("FOUND = %.*s:%.*s\n", XSTR_P(&identity), XSTR_P(&password));
	}
	catch (std::exception& ex)
	{
		printf("EXCEPTION: %s\n", ex.what());
	}
	return 0;
}

#endif
