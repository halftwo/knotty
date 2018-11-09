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
		xlog(XLOG_WARN, "shadow file failed to reload, file=%s, ex=%s", _filename.c_str(), ex.what());
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
		xstr_t b_, g_;
		xstr_key_value(&value, ':', &b_, &value);
		xstr_key_value(&value, ':', &g_, &value);

		// key = bits:g:N
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

		Srp6aParameter srp(bits, g, N);
		_sMap.insert(std::make_pair(key, srp));
	}
	else if (section == SCT_VERIFIER)
	{
		xstr_t method, paramId, hashId, s_;
		xstr_key_value(&value, ':', &method, &value);
		xstr_key_value(&value, ':', &paramId, &value);
		xstr_key_value(&value, ':', &hashId, &value);
		xstr_key_value(&value, ':', &s_, &value);

		if (!xstr_equal_cstr(&method, "SRP6a"))
			throw XERROR_FMT(XError, "Unsupported method in line %d", lineno);

		if (!xstr_case_equal_cstr(&hashId, "SHA256") && !xstr_case_equal_cstr(&hashId, "SHA1"))
			throw XERROR_FMT(XError, "Unsupported hash in line %d", lineno);

		xstr_t salt = ostk_xstr_calloc(_ostk, (s_.len * 3 + 3) / 4);
		salt.len = xbase64_decode(&url_xbase64, salt.data, (char*)s_.data, s_.len, XBASE64_IGNORE_SPACE | XBASE64_NO_PADDING);
		if (salt.len <= 0)
			throw XERROR_FMT(XError, "Decode base64 salt failed in line %d", lineno); 

		int len = value.len - xstr_count_in_bset(&value, &space_bset);
		len = (len * 3 + 3) / 4;
		xstr_t verifier = ostk_xstr_calloc(_ostk, len); 
		verifier.len = xbase64_decode(&url_xbase64, verifier.data, (char*)value.data, value.len, XBASE64_IGNORE_SPACE | XBASE64_NO_PADDING);
		if (verifier.len <= 0)
			throw XERROR_FMT(XError, "Decode base64 verifier failed in line %d", lineno); 

		method = ostk_xstr_dup(_ostk, &method);
		paramId = ostk_xstr_dup(_ostk, &paramId);
		hashId = ostk_xstr_dup(_ostk, &hashId);

		SecretVerifier sv(method, paramId, hashId, salt, verifier);
		_vMap.insert(std::make_pair(key, sv));
	}
	else
	{
		throw XERROR_FMT(XError, "Section not specified until line %d", lineno);
	}
}

void ShadowBox::_add_internal(const char *id, int bits, uintmax_t g, const char *N_str)
{
	assert(id[0] == '@');
	xstr_t paramId = ostk_xstr_dup_cstr(_ostk, id);
	xstr_t N = ostk_xstr_alloc(_ostk, MPSIZE(bits));
	int len = unhexlify_ignore_space(N.data, N_str, -1);
	assert(len == N.len);

	Srp6aParameter srp(bits, g, N);
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

	const char *N4096_str = \
	"ffffffffffffffffc90fdaa22168c234c4c6628b80dc1cd129024e088a67cc74"
	"020bbea63b139b22514a08798e3404ddef9519b3cd3a431b302b0a6df25f1437"
	"4fe1356d6d51c245e485b576625e7ec6f44c42e9a637ed6b0bff5cb6f406b7ed"
	"ee386bfb5a899fa5ae9f24117c4b1fe649286651ece45b3dc2007cb8a163bf05"
	"98da48361c55d39a69163fa8fd24cf5f83655d23dca3ad961c62f356208552bb"
	"9ed529077096966d670c354e4abc9804f1746c08ca18217c32905e462e36ce3b"
	"e39e772c180e86039b2783a2ec07a28fb5c55df06f4c52c9de2bcbf695581718"
	"3995497cea956ae515d2261898fa051015728e5a8aaac42dad33170d04507a33"
	"a85521abdf1cba64ecfb850458dbef0a8aea71575d060c7db3970f85a6e1e4c7"
	"abf5ae8cdb0933d71e8c94e04a25619dcee3d2261ad2ee6bf12ffa06d98a0864"
	"d87602733ec86a64521f2b18177b200cbbe117577a615d6c770988c0bad946e2"
	"08e24fa074e5ab3143db5bfce0fd108e4b82d120a92108011a723c12a787e6d7"
	"88719a10bdba5b2699c327186af4e23c1a946834b6150bda2583e9ca2ad44ce8"
	"dbbbc2db04de8ef92e8efc141fbecaa6287c59474e6bc05d99b2964fa090c3a2"
	"233ba186515be7ed1f612970cee2d7afb81bdd762170481cd0069127d5b05aa9"
	"93b4ea988d8fddc186ffb7dc90a6c08f4df435c934063199ffffffffffffffff";

	_add_internal("@512", 512, 2, N512_str);
	_add_internal("@1024", 1024, 2, N1024_str);
	_add_internal("@2048", 2048, 2, N2048_str);
	_add_internal("@4096", 4096, 5, N4096_str);
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

	for (SecretVerifierMap::iterator iter = _vMap.begin(); iter != _vMap.end(); ++iter)
	{
		const xstr_t& id = iter->first;
		const xstr_t& paramId = iter->second.paramId;

		Srp6aParameterMap::iterator i = _sMap.find(paramId);
		if (i == _sMap.end())
			throw XERROR_FMT(XError, "Unknown paramId `%.*s` with identity `%.*s`", XSTR_P(&paramId), XSTR_P(&id));

		const xstr_t& verifier = iter->second.verifier;
		if (verifier.len != MPSIZE(i->second.bits))
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
	for (Srp6aParameterMap::iterator iter = _sMap.begin(); iter != _sMap.end(); ++iter)
	{
		const xstr_t& paramId = iter->first;
		Srp6aParameter& s = iter->second;

		if (xstr_char_equal(&paramId, 0, '@'))
			continue;

		iobuf_printf(&ob, "%.*s = %d:%jd:\n", XSTR_P(&paramId), s.bits, s.g);

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
	for (SecretVerifierMap::iterator iter = _vMap.begin(); iter != _vMap.end(); ++iter)
	{
		const xstr_t& id = iter->first;
		SecretVerifier& sv = iter->second;

		iobuf_printf(&ob, "!%.*s = %.*s:%.*s:%.*s:", XSTR_P(&id), XSTR_P(&sv.method), XSTR_P(&sv.paramId), XSTR_P(&sv.hashId));

		len = xbase64_encode(&url_xbase64, buf, sv.salt.data, sv.salt.len, XBASE64_NO_PADDING);
		iobuf_write(&ob, buf, len);
		iobuf_puts(&ob, ":\n");

		len = xbase64_encode(&url_xbase64, buf, sv.verifier.data, sv.verifier.len, XBASE64_NO_PADDING);
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

bool ShadowBox::getVerifier(const xstr_t& identity, xstr_t& method, xstr_t& paramId, 
				xstr_t& hashId, xstr_t& salt, xstr_t& verifier)
{
	SecretVerifierMap::iterator iter = _vMap.find(identity);
	if (iter != _vMap.end())
	{
		SecretVerifier& sv = iter->second;
		method = sv.method;
		paramId = sv.paramId;
		hashId = sv.hashId;
		salt = sv.salt;
		verifier = sv.verifier;
		return true;
	}
	return false;
}

bool ShadowBox::getSrp6aParameter(const xstr_t& paramId, int& bits, uintmax_t& g, xstr_t& N)
{
	Srp6aParameterMap::iterator iter = _sMap.find(paramId);
	if (iter != _sMap.end())
	{
		const Srp6aParameter& p = iter->second;
		bits = p.bits;
		g = p.g;
		N = p.N;
		return true;
	}
	return false;
}

Srp6aServerPtr ShadowBox::newSrp6aServer(const xstr_t& paramId, const xstr_t& hashId)
{
	Srp6aServerPtr srp6a;
	Srp6aParameterMap::iterator iter = _sMap.find(paramId);
	if (iter != _sMap.end())
	{
		const Srp6aParameter& p = iter->second;
		srp6a = new Srp6aServer(p.g, p.N, p.bits, hashId);
	}
	return srp6a;
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
