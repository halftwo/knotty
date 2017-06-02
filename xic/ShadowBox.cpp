#include "ShadowBox.h"
#include "xslib/unixfs.h"
#include "xslib/xlog.h"
#include "xslib/hex.h"
#include "xslib/xbase64.h"
#include "xslib/iobuf.h"
#include "xslib/ScopeGuard.h"
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#define MPSIZE(BITS)	(((BITS) + 7) / 8)

ShadowBoxPtr ShadowBox::create()
{
	return new ShadowBox("");
}

ShadowBoxPtr ShadowBox::createFromFile(const std::string& filename)
{
	return new ShadowBox(filename);
}

ShadowBox::ShadowBox(const std::string& filename)
	: _filename(filename), _mtime(0)
{
	try {
		_ostk = ostk_create(0);
		_load();
	}
	catch (...)
	{
		ostk_destroy(_ostk);
		throw;
	}
}

ShadowBox::~ShadowBox()
{
	ostk_destroy(_ostk);
}

ShadowBoxPtr ShadowBox::reload()
{
	ShadowBoxPtr sb;
	try {
		struct stat st;
		if (stat(_filename.c_str(), &st) == 0 && st.st_mtime != _mtime)
		{
			sb = new ShadowBox(_filename);
			xlog(XLOG_NOTICE, "shadow file reloaded, file=%s", _filename.c_str());
		}
	}
	catch (std::exception& ex)
	{
		xlog(XLOG_WARNING, "shadow file failed to reload, file=%s, ex=%s", _filename.c_str(), ex.what());
	}
	return sb;
}

void ShadowBox::_add_item(int lineno, Section section, uint8_t *start, uint8_t *end)
{
	xstr_t xs = XSTR_INIT(start, end - start);
	xstr_t key, value;
	if (xstr_key_value(&xs, '=', &key, &value) <= 0 || key.len == 0 || value.len == 0)
		throw XERROR_FMT(XError, "Invalid syntax in line %d", lineno);

	/* internal parameters are ignored */
	if (xstr_char_equal(&key, 0, '@'))
		return;

	if (xstr_char_equal(&key, 0, '!'))
	{
		// temporarily change the section to SCT_VERIFIER for this item
		section = SCT_VERIFIER;
		xstr_advance(&key, 1);
		if (key.len == 0)
			throw XERROR_FMT(XError, "Invalid syntax in line %d", lineno);
	}

	key = ostk_xstr_dup(_ostk, &key);

	if (section == SCT_SRP6A)
	{
		xstr_t hid, b_, g_;
		xstr_key_value(&value, ':', &hid, &value);
		xstr_key_value(&value, ':', &b_, &value);
		xstr_key_value(&value, ':', &g_, &value);

		if (!xstr_equal_cstr(&hid, "SHA1") && !xstr_equal_cstr(&hid, "SHA256"))
			throw XERROR_FMT(XError, "Unsupported hash `%.*s` in line %d", XSTR_P(&hid), lineno);

		if (value.len == 0)
		{
			// !key = hash:{key2}
			if (b_.len <= 2 || b_.data[0] != '{' || b_.data[b_.len-1] != '}')
				throw XERROR_FMT(XError, "Invalid syntax in line %d", lineno);

			xstr_t xs = xstr_slice(&b_, 1, -1);
			Srp6aMap::iterator iter = _sMap.find(xs);
			if (iter == _sMap.end())
				throw XERROR_FMT(XError, "Unknown reference in line %d", lineno);

			int bits = iter->second.bits;
			uintmax_t g = iter->second.g;
			xstr_t N = iter->second.N;
			Srp6aInfo srp(hid, bits, g, N);
			_sMap.insert(std::make_pair(key, srp));
			return;
		}

		// !key = hash:bits:g:N
		xstr_t end;
		int bits = xstr_to_long(&b_, &end, 10);
		if (bits < 512 || bits > 1024*32 || end.len)
			throw XERROR_FMT(XError, "Invalid bits in line %d", lineno);

		uintmax_t g = xstr_to_integer(&g_, &end, 0);
		if (end.len || g == UINTMAX_MAX)
			throw XERROR_FMT(XError, "Invalid g `%.*s` in line %d", XSTR_P(&g_), lineno);

		int len = value.len - xstr_count_in_bset(&value, &space_bset);
		if (len * 4 < bits)
			throw XERROR_FMT(XError, "Too small N in line %d", lineno);
		else if (len * 4 > bits)
			throw XERROR_FMT(XError, "Too large N in line %d", lineno);

		xstr_t N = ostk_xstr_calloc(_ostk, MPSIZE(bits));
		len = unhexlify_ignore_space(N.data, (char*)value.data, value.len);
		if (len != N.len)
			throw XERROR_FMT(XError, "Invalid N in line %d", lineno);

		hid = ostk_xstr_dup(_ostk, &hid);

		Srp6aInfo srp(hid, bits, g, N);
		_sMap.insert(std::make_pair(key, srp));
	}
	else if (section == SCT_VERIFIER)
	{
		xstr_t method, pid, s_;
		xstr_key_value(&value, ':', &method, &value);
		xstr_key_value(&value, ':', &pid, &value);
		xstr_key_value(&value, ':', &s_, &value);

		if (!xstr_equal_cstr(&method, "SRP6a"))
			throw XERROR_FMT(XError, "Unsupported method in line %d", lineno);

		xstr_t s = ostk_xstr_calloc(_ostk, (s_.len * 3 + 3) / 4);
		s.len = xbase64_decode(&url_xbase64, s.data, (char*)s_.data, s_.len, XBASE64_IGNORE_SPACE | XBASE64_NO_PADDING);
		if (s.len <= 0)
			throw XERROR_FMT(XError, "Decode base64 salt failed in line %d", lineno); 

		int len = value.len - xstr_count_in_bset(&value, &space_bset);
		len = (len * 3 + 3) / 4;
		xstr_t v = ostk_xstr_calloc(_ostk, len); 
		v.len = xbase64_decode(&url_xbase64, v.data, (char*)value.data, value.len, XBASE64_IGNORE_SPACE | XBASE64_NO_PADDING);
		if (v.len <= 0)
			throw XERROR_FMT(XError, "Decode base64 verifier failed in line %d", lineno); 

		method = ostk_xstr_dup(_ostk, &method);
		pid = ostk_xstr_dup(_ostk, &pid);

		Verifier verifier(method, pid, s, v);
		_vMap.insert(std::make_pair(key, verifier));
	}
	else
	{
		throw XERROR_FMT(XError, "Section not specified until line %d", lineno);
	}
}

void ShadowBox::_add_internal(const char *id, const char *hash, int bits, uintmax_t g, const char *N_str)
{
	xstr_t paramId = ostk_xstr_dup_cstr(_ostk, id);
	xstr_t hid = ostk_xstr_dup_cstr(_ostk, hash);
	xstr_t N = ostk_xstr_alloc(_ostk, MPSIZE(bits));
	int len = unhexlify_ignore_space(N.data, N_str, -1);
	assert(len == N.len);

	Srp6aInfo srp(hid, bits, g, N);
	_sMap.insert(std::make_pair(paramId, srp));
}

void ShadowBox::_add_internal_parameters()
{
	const char *N512_str = \
	"d4c7f8a2b32c11b8fba9581ec4ba4f1b04215642ef7355e37c0fc0443ef756ea"
	"2c6b8eeb755a1c723027663caa265ef785b8ff6a9b35227a52d86633dbdfca43";

	const char *N1024_str = \
	"eeaf0ab9adb38dd69c33f80afa8fc5e86072618775ff3c0b9ea2314c9c256576"
	"d674df7496ea81d3383b4813d692c6e0e0d5d8e250b98be48e495c1d6089dad1"
	"5dc7d7b46154d6b6ce8ef4ad69b15d4982559b297bcf1885c529f566660e57ec"
	"68edbc3c05726cc02fd4cbf4976eaa9afd5138fe8376435b9fc61d2fc0eb06e3";

	const char *N2048_str = \
	"ac6bdb41324a9a9bf166de5e1389582faf72b6651987ee07fc3192943db56050"
	"a37329cbb4a099ed8193e0757767a13dd52312ab4b03310dcd7f48a9da04fd50"
	"e8083969edb767b0cf6095179a163ab3661a05fbd5faaae82918a9962f0b93b8"
	"55f97993ec975eeaa80d740adbf4ff747359d041d5c33ea71d281e446b14773b"
	"ca97b43a23fb801676bd207a436c6481f1d2b9078717461a5b9d32e688f87748"
	"544523b524b0d57d5ea77a2775d2ecfa032cfbdbf52fb3786160279004e57ae6"
	"af874e7303ce53299ccc041c7bc308d82a5698f3a8d0c38271ae35f8e9dbfbb6"
	"94b5c803d89f7ae435de236d525f54759b65e372fcd68ef20fa7111f9e4aff73";

	// NB: '*' as paramId is deprecated, will be removed later.
	// DONT use it.
	_add_internal("*", "SHA1", 1024, 2, N1024_str);

	_add_internal("@512SHA256", "SHA256", 512, 2, N512_str);
	_add_internal("@1024SHA256", "SHA256", 1024, 2, N1024_str);
	_add_internal("@2048SHA256", "SHA256", 2048, 2, N2048_str);
}

void ShadowBox::_load()
{
	_add_internal_parameters();

	if (_filename.empty())
		return;

	struct stat st;
	if (stat(_filename.c_str(), &st) < 0)
		throw XERROR_FMT(XError, "stat() failed, file=%s, errno=%d", _filename.c_str(), errno);

	_mtime = st.st_mtime;

	uint8_t *buf = NULL;
	size_t size = 0;
	ssize_t len = unixfs_get_content(_filename.c_str(), &buf, &size);
	if (len < 0)
		throw XERROR_FMT(XError, "Failed to get content of file `%s`", _filename.c_str());
	ON_BLOCK_EXIT(free, buf);

	int lineno = 0;
	Section section = SCT_UNKNOWN;
	xstr_t xs = XSTR_INIT(buf, len);
	xstr_t line;
	uint8_t *start = NULL;
	int saved_lineno = 0;
	while (xstr_delimit_char(&xs, '\n', &line))
	{
		++lineno;
		xstr_rtrim(&line);

		if (line.len == 0 || line.data[0] == '#')
		{
			if (start)
				_add_item(saved_lineno, section, start, line.data);

			start = NULL;
		}
		else if (isspace(line.data[0]))
		{
			/* Do nothing.
			 * lines start with space are continued from previous line
			 */
		}
		else if (line.data[0] == '[')
		{
			if (xstr_equal_cstr(&line, "[SRP6a]"))
			{
				section = SCT_SRP6A;
			}
			else if (xstr_equal_cstr(&line, "[verifier]"))
			{
				section = SCT_VERIFIER;
			}
			else
			{
				section = SCT_UNKNOWN;
				throw XERROR_FMT(XError, "Unknown section `%.*s` in line %d", XSTR_P(&line), lineno);
			}

			if (start)
				_add_item(saved_lineno, section, start, line.data);

			start = NULL;
		}
		else
		{
			if (xstr_find_char(&line, 0, '=') < 0)
				throw XERROR_FMT(XError, "Invalid syntax in line %d", lineno);

			if (start)
				_add_item(saved_lineno, section, start, line.data);

			start = line.data;
			saved_lineno = lineno;
		}
	}

	if (start)
		_add_item(saved_lineno, section, start, buf + len);

	for (VerifierMap::iterator iter = _vMap.begin(); iter != _vMap.end(); ++iter)
	{
		const xstr_t& id = iter->first;
		const xstr_t& pid = iter->second.paramId;

		Srp6aMap::iterator i = _sMap.find(pid);
		if (i == _sMap.end())
			throw XERROR_FMT(XError, "Unknown paramId `%.*s` with identity `%.*s`", XSTR_P(&pid), XSTR_P(&id));

		const xstr_t& v = iter->second.v;
		if (v.len != MPSIZE(i->second.bits))
			throw XERROR_FMT(XError, "Invalid verifier data with identity `%.*s`", XSTR_P(&id));
	}
}

void ShadowBox::dump(xio_write_function write, void *cookie)
{
	int i, len;
	char buf[8192+1];
	uint8_t tmpbuf[4096];
	xio_t myio = { NULL, write, NULL, NULL };
	iobuf_t ob = IOBUF_INIT(&myio, cookie, tmpbuf, sizeof(tmpbuf));

	iobuf_puts(&ob, "[SRP6a]\n\n");
	for (Srp6aMap::iterator iter = _sMap.begin(); iter != _sMap.end(); ++iter)
	{
		const xstr_t& pid = iter->first;
		Srp6aInfo& s = iter->second;

		if (xstr_char_equal(&pid, 0, '@'))
			continue;

		iobuf_printf(&ob, "%.*s = %.*s:%d:%jd:\n", XSTR_P(&pid), XSTR_P(&s.hashId), s.bits, s.g);

		len = hexlify_upper(buf, s.N.data, s.N.len);
		for (i = 0; i < len; i += 64)
		{
			int n = len - i;
			if (n > 64)
				n = 64;
			iobuf_puts(&ob, "        ");
			iobuf_write(&ob, buf + i, n);
			iobuf_putc(&ob, '\n');
		}
		iobuf_putc(&ob, '\n');
	}

	iobuf_puts(&ob, "[verifier]\n\n");
	for (VerifierMap::iterator iter = _vMap.begin(); iter != _vMap.end(); ++iter)
	{
		const xstr_t& id = iter->first;
		Verifier& v = iter->second;

		iobuf_printf(&ob, "%.*s = %.*s:%.*s:", XSTR_P(&id), XSTR_P(&v.method), XSTR_P(&v.paramId));

		len = xbase64_encode(&url_xbase64, buf, v.s.data, v.s.len, XBASE64_NO_PADDING);
		iobuf_write(&ob, buf, len);
		iobuf_puts(&ob, ":\n");

		len = xbase64_encode(&url_xbase64, buf, v.v.data, v.v.len, XBASE64_NO_PADDING);
		for (i = 0; i < len; i += 64)
		{
			int n = len - i;
			if (n > 64)
				n = 64;
			iobuf_puts(&ob, "        ");
			iobuf_write(&ob, buf + i, n);
			iobuf_putc(&ob, '\n');
		}
		iobuf_putc(&ob, '\n');
	}

	iobuf_finish(&ob);
}

bool ShadowBox::getVerifier(const xstr_t& identity, xstr_t& method, xstr_t& paramId, xstr_t& s, xstr_t& v)
{
	VerifierMap::iterator iter = _vMap.find(identity);
	if (iter != _vMap.end())
	{
		Verifier& verifier = iter->second;
		method = verifier.method;
		paramId = verifier.paramId;
		s = verifier.s;
		v = verifier.v;
		return true;
	}
	return false;
}

bool ShadowBox::getSrp6aParameter(const xstr_t& paramId, xstr_t& hashId, int& bits, uintmax_t& g, xstr_t& N)
{
	Srp6aMap::iterator iter = _sMap.find(paramId);
	if (iter != _sMap.end())
	{
		Srp6aInfo& srp = iter->second;
		hashId = srp.hashId;
		bits = srp.bits;
		g = srp.g;
		N = srp.N;
		return true;
	}
	return false;
}

Srp6aServerPtr ShadowBox::newSrp6aServer(const xstr_t& paramId)
{
	Srp6aServerPtr s;
	Srp6aMap::iterator iter = _sMap.find(paramId);
	if (iter != _sMap.end())
		s = new Srp6aServer(*iter->second.srp6a);
	return s;
}


#ifdef TEST_SHADOW

#include <stdio.h>

int main(int argc, char **argv)
{
	try {
		if (argc < 2)
		{
			fprintf(stderr, "Usage: %s file\n", argv[0]);
			exit(1);
		}

		ShadowBoxPtr sb = ShadowBox::createFromFile(argv[1]);
		sb->dump(stdio_xio.write, stdout);
	}
	catch (std::exception& ex)
	{
		printf("EXCEPTION: %s\n", ex.what());
	}
	return 0;
}

#endif
