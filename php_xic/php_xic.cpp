#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_xic.h"
#include "xs_XError.h"
#include "xic_Exception.h"
#include "xic_Engine.h"
#include "xic_Proxy.h"
#include "vbs_Blob.h"
#include "vbs_Dict.h"
#include "vbs_Decimal.h"
#include "vbs_Data.h"
#include "vbs_codec.h"
#include "smart_write.h"
#include "ext/standard/info.h"
#include "Zend/zend_smart_str.h"
#include "xslib/vbs_pack.h"
#include "xslib/xsdef.h"
#include "xslib/xbase57.h"
#include "dlog/dlog.h"

ZEND_DECLARE_MODULE_GLOBALS(xic)

/* True global resources - no need for thread safety here */
static int le_xic;

/*
  See:
    https://wiki.php.net/internals/extensions
  for the difference of php extensions (modules) and zend extensions.

  NB, We are providing a php extension (module) instead of a zend extension.
*/
static char phpxic_version[] = "$phpxic: " XIC_SO_VERSION "-" PHP_XIC_VERSION " module_api=" XS_TOSTR(ZEND_MODULE_API_NO) " " __DATE__ " " __TIME__ " $";

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_build_info, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_engine, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_self_id, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_self, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_cid, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_set_cid, 0, 0, 1)
	ZEND_ARG_INFO(0, cid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_rid, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xic_set_rid, 0, 0, 1)
	ZEND_ARG_INFO(0, rid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_blob, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_dict, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_decimal, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_data, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
	ZEND_ARG_INFO(0, descriptor)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_encode, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_encode_write, 0, 0, 2)
	ZEND_ARG_INFO(0, handle)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_decode, 0, 0, 1)
	ZEND_ARG_INFO(0, vbs)
	ZEND_ARG_INFO(1, used)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_pack, 0, 0, 1)
	ZEND_ARG_INFO(0, values)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_unpack, 0, 0, 3)
	ZEND_ARG_INFO(0, vbs)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, num)
	ZEND_ARG_INFO(1, used)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_dlog, 0, 0, 3)
	ZEND_ARG_INFO(0, identity)
	ZEND_ARG_INFO(0, tag)
	ZEND_ARG_INFO(0, content)
ZEND_END_ARG_INFO()

/* {{{ xic_functions[]
 *
 * Every user visible function must have an entry in xic_functions[].
 */
zend_function_entry xic_functions[] = {
	PHP_FE(xic_build_info, arginfo_xic_build_info)
	PHP_FE(xic_engine, arginfo_xic_engine)
	PHP_FE(xic_self_id, arginfo_xic_self_id)
	PHP_FE(xic_self, arginfo_xic_self)
	PHP_FE(xic_cid, arginfo_xic_cid)
	PHP_FE(xic_set_cid, arginfo_xic_set_cid)
	PHP_FE(xic_rid, arginfo_xic_rid)
	PHP_FE(xic_set_rid, arginfo_xic_set_rid)
	PHP_FE(vbs_blob, arginfo_vbs_blob)
	PHP_FE(vbs_dict, arginfo_vbs_dict)
	PHP_FE(vbs_decimal, arginfo_vbs_decimal)
	PHP_FE(vbs_data, arginfo_vbs_data)
	PHP_FE(vbs_encode, arginfo_vbs_encode)
	PHP_FE(vbs_decode, arginfo_vbs_decode)
	PHP_FE(vbs_encode_write, arginfo_vbs_encode_write)
	PHP_FE(vbs_pack, arginfo_vbs_pack)
	PHP_FE(vbs_unpack, arginfo_vbs_unpack)
	PHP_FE(dlog, arginfo_dlog)
	{NULL, NULL, NULL}	/* Must be the last line in xic_functions[] */
};
/* }}} */

/* {{{ xic_module_entry
 */
zend_module_entry xic_module_entry = {
	STANDARD_MODULE_HEADER,
	"xic",
	xic_functions,
	PHP_MINIT(xic),
	PHP_MSHUTDOWN(xic),
	PHP_RINIT(xic),
	PHP_RSHUTDOWN(xic),
	PHP_MINFO(xic),
	XIC_SO_VERSION, /* Replace with version number for your extension */
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_XIC
ZEND_GET_MODULE(xic)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("xic.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_xic_globals, xic_globals)
    STD_PHP_INI_ENTRY("xic.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_xic_globals, xic_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_xic_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_xic_init_globals(zend_xic_globals *xic_globals)
{
}
*/
/* }}} */


PHP_FUNCTION(xic_build_info)
{
	RETURN_STRING(phpxic_version);
}

zval *get_xic_self_id()
{
	zval* self_id = &XIC_G(the_self_id);
	if (Z_ISUNDEF(*self_id))
	{
		char id[20];
		int len = get_self_process_id(id, sizeof(id));
		ZVAL_STRINGL(self_id, id, len);
	}
	return self_id;
}

zval *get_xic_self()
{
	zval* self = &XIC_G(the_self);
	if (Z_ISUNDEF(*self))
	{
		zval* rid = get_xic_rid();
		std::string s = get_self_process(Z_STRVAL_P(rid));
		ZVAL_STRINGL(self, (char *)s.data(), s.length());
	}
	return self;
}

zval *get_xic_cid()
{
	zval* cid = &XIC_G(the_cid);
	if (Z_ISUNDEF(*cid))
	{
		char buf[18];
		int len = generate_xic_cid(buf);
		ZVAL_STRINGL(cid, buf, len);
	}
	return cid;
}

zval *get_xic_rid()
{
	zval* rid = &XIC_G(the_rid);
	if (Z_ISUNDEF(*rid))
	{
		char buf[24];
		int len = generate_xic_rid(buf, NULL, 0);
		ZVAL_STRINGL(rid, buf, len);
	}
	return rid;
}

/* proto object xic_engine()
   Return the xic Engine object 
   If error, return NULL.
*/
PHP_FUNCTION(xic_engine)
{
	zval* zv = &XIC_G(the_engine);
	if (Z_ISUNDEF(*zv))
	{
		xic::EnginePtr engine(new xic::Engine());
		xic::create_Engine(zv, engine TSRMLS_CC);
	}

	RETURN_ZVAL(zv, 1, 0);
}


/* proto string xic_self_id()
*/
PHP_FUNCTION(xic_self_id)
{
	zval* zv = get_xic_self_id();
	RETURN_ZVAL(zv, 1, 0);
}


/* proto string xic_self()
*/
PHP_FUNCTION(xic_self)
{
	zval* zv = get_xic_self();
	RETURN_ZVAL(zv, 1, 0);
}


/* proto string xic_cid();
 */
PHP_FUNCTION(xic_cid)
{
	zval* cid = &XIC_G(the_cid);
	if (!Z_ISUNDEF(*cid))
		RETURN_ZVAL(cid, 1, 0);

	RETVAL_STRINGL(NULL, 0);
}


/* proto void xic_set_cid(string $cid);
 */
PHP_FUNCTION(xic_set_cid)
{
	char *cid = NULL;
	size_t len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s/", &cid, &len) != SUCCESS)
	{
		raise_Exception(0 TSRMLS_CC, "Wrong parameters for xic_set_cid(string $cid)");
	}

	if (len > 0 && (len != 17 || strspn(cid, xbase57_alphabet) < len 
		|| (strchr(xbase57_alphabet, cid[0]) - xbase57_alphabet >= 49)))
	{
		raise_Exception(0 TSRMLS_CC, "Invalid cid for xic_set_cid(string $cid)");
	}

	char buf[18];
	if (len <= 0)
	{
		cid = buf;
		len = generate_xic_cid(buf);
	}

	zval* zv = &XIC_G(the_cid);
	if (!Z_ISUNDEF(*zv))
	{
		zval_ptr_dtor(zv);
	}

	ZVAL_STRINGL(zv, cid, len);
}


/* proto string xic_rid();
 */
PHP_FUNCTION(xic_rid)
{
	zval* rid = &XIC_G(the_rid);
	if (!Z_ISUNDEF(*rid))
		RETURN_ZVAL(rid, 1, 0);

	RETVAL_STRINGL(NULL, 0);
}


/* proto void xic_set_rid(string $rid);
 */
PHP_FUNCTION(xic_set_rid)
{
	char *rid = NULL;
	size_t len = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s/", &rid, &len) != SUCCESS)
	{
		raise_Exception(0 TSRMLS_CC, "Wrong parameters for xic_set_rid(string $rid)");
	}

	bool valid = false;
	if (len >= 11 && len <= 23)
	{
		size_t n = strspn(rid, xbase57_alphabet);
		if (n >= 11)
		{
			if (n == len)
			{
				valid = true;
			}
			else if (n < len - 1 && rid[n] == '-')
			{
				size_t k = strspn(rid + n + 1, xbase57_alphabet);
				if (n + 1 + k == len)
				{
					valid = true;
				}
			}
		}
	}
	else if (len == 0)
	{
		valid = true;
	}

	if (!valid)
	{
		raise_Exception(0 TSRMLS_CC, "Invalid rid for xic_set_rid(string $rid)");
	}

	char buf[24];
	if (len <= 0)
	{
		rid = buf;
		len = generate_xic_rid(buf, NULL, 0);
	}

	zval* zv = &XIC_G(the_rid);
	if (!Z_ISUNDEF(*zv))
	{
		zval_ptr_dtor(zv);
	}

	ZVAL_STRINGL(zv, rid, len);
}

PHP_FUNCTION(vbs_blob)
{
	zval *z;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &z) != SUCCESS)
	{
		raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_blob(string $s)");
	}
	else if (Z_TYPE_P(z) == IS_STRING)
	{
		vbs::create_Blob(return_value, z TSRMLS_CC);
	}
	else if (VALID_OBJECT(z, vbs::classEntry_Blob))
	{
		zval rv;
		ZVAL_UNDEF(&rv);
		z = zend_read_property(vbs::classEntry_Blob, MY_Z_OBJ_P(z), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
		vbs::create_Blob(return_value, z TSRMLS_CC);
	}
	else
	{
		raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_blob(string $s)");
	}
}

PHP_FUNCTION(vbs_dict)
{
	zval *z = NULL;
	if (ZEND_NUM_ARGS() > 0)
	{
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &z) != SUCCESS)
		{
			raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_dict(array $a)");
		}
		else if (Z_TYPE_P(z) == IS_ARRAY)
		{
			vbs::create_Dict(return_value, z TSRMLS_CC);
		}
		else if (VALID_OBJECT(z, vbs::classEntry_Dict))
		{
			zval rv;
			ZVAL_UNDEF(&rv);
			z = zend_read_property(vbs::classEntry_Dict, MY_Z_OBJ_P(z), "a", sizeof("a") - 1, 0, &rv TSRMLS_CC);
			vbs::create_Dict(return_value, z TSRMLS_CC);
		}
		else
		{
			raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_dict(array $a)");
		}
	}
	else
	{
		zval rv;
		ZVAL_UNDEF(&rv);
		array_init(&rv);
		vbs::create_Dict(return_value, &rv TSRMLS_CC);
	}
}

PHP_FUNCTION(vbs_decimal)
{
	zval *z;

	try {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &z) != SUCCESS)
		{
			raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_decimal(string $s)");
		}
		else if (Z_TYPE_P(z) == IS_STRING || Z_TYPE_P(z) == IS_LONG || Z_TYPE_P(z) == IS_DOUBLE)
		{
			vbs::create_Decimal(return_value, z TSRMLS_CC);
		}
		else if (VALID_OBJECT(z, vbs::classEntry_Decimal))
		{
			zval rv;
			ZVAL_UNDEF(&rv);
			z = zend_read_property(vbs::classEntry_Decimal, MY_Z_OBJ_P(z), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
			vbs::create_Decimal(return_value, z TSRMLS_CC);
		}
		else
		{
			raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_decimal(string $s)");
		}
	}
	catch (std::exception& ex)
	{
		raise_Exception(0 TSRMLS_CC, "%s", ex.what());
	}
}

PHP_FUNCTION(vbs_data)
{
	zval *dat;
	zend_long descriptor = 0;
	zend_long max = VBS_DESCRIPTOR_MAX | VBS_SPECIAL_DESCRIPTOR;

	try {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/l", &dat, &descriptor) != SUCCESS)
		{
			raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_data(mixed $data, int $descriptor)");
		}
		else if (descriptor < 0 || descriptor > max)
		{
			raise_Exception(0 TSRMLS_CC, "descriptor of vbs_data should be a positive integer not greater than %d", VBS_DESCRIPTOR_MAX);
		}

		vbs::create_Data(return_value, dat, descriptor TSRMLS_CC);
	}
	catch (std::exception& ex)
	{
		raise_Exception(0 TSRMLS_CC, "%s", ex.what());
	}
}

PHP_FUNCTION(vbs_encode)
{
	smart_str ss = {0};
	vbs_packer_t pk;
	zval *val;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &val) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for blob vbs_encode(mixed $value)");
	}

	try {
		vbs_packer_init(&pk, smart_write, &ss, -1);
		vbs::v_encode_r(&pk, val TSRMLS_CC);
		RETVAL_STR(ss.s);
	}
	catch (std::exception& ex)
	{
		raise_Exception(0 TSRMLS_CC, "%s", ex.what());
	}
}

PHP_FUNCTION(vbs_encode_write)
{
	vbs_packer_t pk;
	zval *fp;
	zval *val;
	struct file_cookie fcookie;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &fp, &val) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for int vbs_encode_write(resource $handle, mixed $value)");
	}

	try {
		file_cookie_init(&fcookie);
		php_stream_from_zval(fcookie.stream, fp);

		vbs_packer_init(&pk, file_write, &fcookie, -1);
		vbs::v_encode_r(&pk, val TSRMLS_CC);

		if (pk.error)
		{
			RETURN_FALSE;
		}

		file_cookie_finish(&fcookie);
		ZVAL_LONG(return_value, fcookie.total);
	}
	catch (std::exception& ex)
	{
		raise_Exception(0 TSRMLS_CC, "%s", ex.what());
	}
}

PHP_FUNCTION(vbs_decode)
{
	zval *z = NULL;
	zval *used = NULL;
	vbs_unpacker_t uk;
	char *str = NULL;
	int slen = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z/", &z, &used) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for mixed vbs_decode(blob $vbs [, int& $used])");
	}
	else if (Z_TYPE_P(z) == IS_STRING)
	{
		str = Z_STRVAL_P(z);
		slen = Z_STRLEN_P(z);
	}
	else if (VALID_OBJECT(z, vbs::classEntry_Blob))
	{
		zval rv;
		ZVAL_UNDEF(&rv);
		z = zend_read_property(vbs::classEntry_Blob, MY_Z_OBJ_P(z), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
		str = Z_STRVAL_P(z);
		slen = Z_STRLEN_P(z);
	}
	else
	{
		goto error;
	}

	vbs_unpacker_init(&uk, (unsigned char *)str, slen, -1);
	zval rv;
	ZVAL_UNDEF(&rv);
	if (!vbs::v_decode_r(&uk, &rv TSRMLS_CC))
	{
		goto error;
	}

	ZVAL_COPY_VALUE(return_value, &rv);
	if (used)
		ZVAL_LONG(used, uk.cur - uk.buf);
	return;

error:
	if (used)
			ZVAL_LONG(used, 0);
	ZVAL_NULL(return_value);
}


PHP_FUNCTION(vbs_pack)
{
	smart_str ss = {0};
	vbs_packer_t pk;
	zval *val;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &val) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for blob vbs_pack(array $values)");
	}

	try {
		vbs_packer_init(&pk, smart_write, &ss, -1);
		vbs::v_pack(&pk, val TSRMLS_CC);

		RETVAL_STR(ss.s);
	}
	catch (std::exception& ex)
	{
		raise_Exception(0 TSRMLS_CC, "%s", ex.what());
	}
}

PHP_FUNCTION(vbs_unpack)
{
	zval *z = NULL;
	zend_long offset = 0;
	zend_long num = 0;
	zval *used = NULL;
	
	vbs_unpacker_t uk;
	char *str = NULL;
	int slen = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zll|z/", &z, &offset, &num, &used) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for array vbs_unpack(blob $vbs, int $offset, int $num [, int& $used])");
	}
	else if (Z_TYPE_P(z) == IS_STRING)
	{
		str = Z_STRVAL_P(z);
		slen = Z_STRLEN_P(z);
	}
	else if (Z_TYPE_P(z) == IS_OBJECT && Z_OBJCE_P(z) == vbs::classEntry_Blob)
	{
		zval rv;
		ZVAL_UNDEF(&rv);
		z = zend_read_property(vbs::classEntry_Blob, MY_Z_OBJ_P(z), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
		str = Z_STRVAL_P(z);
		slen = Z_STRLEN_P(z);
	}
	else
	{
		goto error;
	}

	if (offset >= slen)
	{
		goto error;
	}

	str += offset;
	slen -= offset;

	vbs_unpacker_init(&uk, (unsigned char *)str, slen, -1);
	zval rv;
	ZVAL_UNDEF(&rv);
	if (!vbs::v_unpack(&uk, num, &rv TSRMLS_CC))
	{
		goto error;
	}

	ZVAL_COPY_VALUE(return_value, &rv);
	if (used)
		ZVAL_LONG(used, uk.cur - uk.buf);
	return;

error:
	if (used)
			ZVAL_LONG(used, 0);
	ZVAL_NULL(return_value);
}

static void copymem(char **ptr, char *end, const char *src, int n)
{
	if (*ptr < end)
	{
		int left = end - *ptr;
		if (n > left)
			n = left;
		memcpy(*ptr, src, n);
		*ptr += n;
	}
}

PHP_FUNCTION(dlog)
{
	char *identity = NULL, *tag = NULL;
	zval *content = NULL;
	size_t ilen = 0, tlen = 0, clen = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssz",
			&identity, &ilen, &tag, &tlen, &content) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for void dlog(string $identity, string $tag, mixed $content)");
	}

	xstr_t tag_xs = XSTR_INIT((unsigned char *)tag, tlen);

	char idbuf[64];
	char *p = idbuf, *end = idbuf + sizeof(idbuf) - 1;
	zval *rid = get_xic_rid();
	copymem(&p, end, "PHP+", 4);
	copymem(&p, end, Z_STRVAL_P(rid), Z_STRLEN_P(rid));
	if (ilen > 0)
	{
		copymem(&p, end, "+", 1);
		copymem(&p, end, identity, ilen);
	}
	*p = 0;
	xstr_t identity_xs = XSTR_INIT((unsigned char*)idbuf, p - idbuf);

	const char *filename = zend_get_executed_filename(TSRMLS_CC);
	int lineno = zend_get_executed_lineno(TSRMLS_CC);
	char lobuf[128];
	int llen = snprintf(lobuf, sizeof(lobuf), "%s:%d", filename, lineno);
	if (llen >= sizeof(lobuf))
	{
		int extra = (llen - sizeof(lobuf)) + sizeof("...");
		const char *mark = filename + extra;
		const char *s = filename;
		for (const char *p; (p = strchr(s, '/')) != NULL; s = p + 1)
		{
			if (p >= mark)
			{
				s = p;
				break;
			}
		}

		if (s < mark)
			s = mark;

		llen = snprintf(lobuf, sizeof(lobuf), "...%s:%d", s, lineno);
	}

	xstr_t locus_xs = XSTR_INIT((unsigned char *)lobuf, llen);

	if (Z_TYPE_P(content) == IS_STRING)
	{
		char *s = Z_STRVAL_P(content);
		int len = Z_STRLEN_P(content);
		xstr_t content_xs = XSTR_INIT((unsigned char *)s, len);
		zdlog(&identity_xs, &tag_xs, &locus_xs, &content_xs);
	}
	else
	{
		try {
			smart_str ss = {0};
			vbs_packer_t pk;
			vbs_packer_init(&pk, smart_write, &ss, -1);
			vbs::v_encode_r(&pk, content TSRMLS_CC);
			zend_string *s = ss.s;
			xstr_t content_xs = XSTR_INIT((unsigned char *)ZSTR_VAL(s), ZSTR_LEN(s));
			xdlog(vbs_xfmt, idbuf, tag, lobuf, "%p{>VBS_RAW<}", &content_xs);
			smart_str_free(&ss);
		}
		catch (std::exception& ex)
		{
			raise_Exception(0 TSRMLS_CC, "%s", ex.what());
		}
	}
}

static void php_xic_init_globals(zend_xic_globals *xic_globals TSRMLS_DC)
{
	ZVAL_UNDEF(&xic_globals->the_engine);
	ZVAL_UNDEF(&xic_globals->the_self);
	ZVAL_UNDEF(&xic_globals->the_self_id);
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(xic)
{
	ZEND_INIT_MODULE_GLOBALS(xic, php_xic_init_globals, NULL);
	xs::init_XError(TSRMLS_C);
	xic::init_Exception(TSRMLS_C);
	xic::init_Engine(TSRMLS_C);
	xic::init_Proxy(TSRMLS_C);
	vbs::init_Blob(TSRMLS_C);
	vbs::init_Dict(TSRMLS_C);
	vbs::init_Decimal(TSRMLS_C);
	vbs::init_Data(TSRMLS_C);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(xic)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(xic)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "xic support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(xic)
{
	get_xic_self();
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(xic)
{
	if (!Z_ISUNDEF(XIC_G(the_engine)))
	{
		zval* zv = &XIC_G(the_engine);
		xic::EnginePtr engine = MyObject<xic::Engine>::get(zv TSRMLS_CC); 
		engine->finish();
		zval_ptr_dtor(zv);
		ZVAL_UNDEF(zv);
	}

	if (!Z_ISUNDEF(XIC_G(the_self)))
	{
		zval* zv = &XIC_G(the_self);
		zval_ptr_dtor(zv);
		ZVAL_UNDEF(zv);
	}

	if (!Z_ISUNDEF(XIC_G(the_self_id)))
	{
		zval* zv = &XIC_G(the_self_id);
		zval_ptr_dtor(zv);
		ZVAL_UNDEF(zv);
	}

	if (!Z_ISUNDEF(XIC_G(the_cid)))
	{
		zval* zv = &XIC_G(the_cid);
		zval_ptr_dtor(zv);
		ZVAL_UNDEF(zv);
	}

	if (!Z_ISUNDEF(XIC_G(the_rid)))
	{
		zval* zv = &XIC_G(the_rid);
		zval_ptr_dtor(zv);
		ZVAL_UNDEF(zv);
	}

	return SUCCESS;
}
/* }}} */


/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
