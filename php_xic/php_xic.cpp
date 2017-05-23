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
#include "ext/standard/php_smart_str.h"
#include "xslib/vbs_pack.h"
#include "xslib/xsdef.h"
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
static char version[] = "$PHPXIC: " XIC_SO_VERSION "-" PHP_XIC_VERSION " module_api=" XS_TOSTR(ZEND_MODULE_API_NO) " " __DATE__ " " __TIME__ " $";

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_encode, 0, 0, 1)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_encode_write, 0, 0, 1)
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_vbs_unpack, 0, 0, 1)
	ZEND_ARG_INFO(0, vbs)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, num)
	ZEND_ARG_INFO(1, used)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_dlog, 0, 0, 1)
	ZEND_ARG_INFO(0, identity)
	ZEND_ARG_INFO(0, tag)
	ZEND_ARG_INFO(0, content)
ZEND_END_ARG_INFO()

/* {{{ xic_functions[]
 *
 * Every user visible function must have an entry in xic_functions[].
 */
zend_function_entry xic_functions[] = {
	PHP_FE(xic_build_info, NULL)
	PHP_FE(xic_engine, NULL)
	PHP_FE(xic_self_id, NULL)
	PHP_FE(xic_self, NULL)
	PHP_FE(vbs_blob, NULL)
	PHP_FE(vbs_dict, NULL)
	PHP_FE(vbs_decimal, NULL)
	PHP_FE(vbs_data, NULL)
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
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"xic",
	xic_functions,
	PHP_MINIT(xic),
	PHP_MSHUTDOWN(xic),
	PHP_RINIT(xic),
	PHP_RSHUTDOWN(xic),
	PHP_MINFO(xic),
#if ZEND_MODULE_API_NO >= 20010901
	XIC_SO_VERSION, /* Replace with version number for your extension */
#endif
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
	xic_globals->global_value = 0;
	xic_globals->global_string = NULL;
}
*/
/* }}} */


PHP_FUNCTION(xic_build_info)
{
	RETURN_STRING((char *)version, 1);
}

zval *get_xic_self_id()
{
	zval* self_id = XIC_G(the_self_id);
	if (!self_id)
	{
		char id[16];
		int len = get_self_process_id(id, sizeof(id));
		MAKE_STD_ZVAL(self_id);
		ZVAL_STRINGL(self_id, id, len, 1);
		XIC_G(the_self_id) = self_id;
	}
	return self_id;
}

zval *get_xic_self()
{
	zval* self = XIC_G(the_self);
	if (!self)
	{
		zval* id = get_xic_self_id();
		std::string s = get_self_process(Z_STRVAL_P(id));
		MAKE_STD_ZVAL(self);
		ZVAL_STRINGL(self, (char *)s.data(), s.length(), 1);
		XIC_G(the_self) = self;
	}
	return self;
}

/* proto object xic_engine()
   Return the xic Engine object 
   If error, return NULL.
*/
PHP_FUNCTION(xic_engine)
{
	zval* zv = XIC_G(the_engine);
	if (!zv)
	{
		MAKE_STD_ZVAL(zv);
		xic::EnginePtr engine(new xic::Engine());
		xic::create_Engine(zv, engine TSRMLS_CC);
		XIC_G(the_engine) = zv;
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
		z = zend_read_property(vbs::classEntry_Blob, z, "s", sizeof("s") - 1, 0 TSRMLS_CC);
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
			z = zend_read_property(vbs::classEntry_Dict, z, "a", sizeof("a") - 1, 0 TSRMLS_CC);
			vbs::create_Dict(return_value, z TSRMLS_CC);
		}
		else
		{
			raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_dict(array $a)");
		}
	}
	else
	{
		MAKE_STD_ZVAL(z);
		array_init(z);
		vbs::create_Dict(return_value, z TSRMLS_CC);
		ZVAL_DELREF(z);
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
			z = zend_read_property(vbs::classEntry_Decimal, z, "s", sizeof("s") - 1, 0 TSRMLS_CC);
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
	long descriptor;
	long max = VBS_DESCRIPTOR_MAX | VBS_SPECIAL_DESCRIPTOR;

	try {
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/l", &dat, &descriptor) != SUCCESS)
		{
			raise_Exception(0 TSRMLS_CC, "Wrong parameters for vbs_data(mixed $d, int descriptor)");
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
	smart_str buf = {0};
	vbs_packer_t pk;
	zval *val;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &val) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for blob vbs_encode(mixed $value)");
	}

	try {
		vbs_packer_init(&pk, smart_write, &buf, -1);
		vbs::v_encode_r(&pk, val TSRMLS_CC);

		ZVAL_STRINGL(return_value, buf.c, buf.len, 1);																  
		smart_str_free(&buf);
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
		php_stream_from_zval(fcookie.stream, &fp);

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
	zval *r = NULL;
	vbs_unpacker_t uk;
	char *str = NULL;
	int slen = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|z", &z, &used) == FAILURE)
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
		z = zend_read_property(vbs::classEntry_Blob, z, "s", sizeof("s") - 1, 0 TSRMLS_CC);
		str = Z_STRVAL_P(z);
		slen = Z_STRLEN_P(z);
	}
	else
	{
		goto error;
	}

	vbs_unpacker_init(&uk, (unsigned char *)str, slen, -1);
	if (!vbs::v_decode_r(&uk, &r TSRMLS_CC))
	{
		goto error;
	}

	*return_value = *r;
	FREE_ZVAL(r);
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
	smart_str buf = {0};
	vbs_packer_t pk;
	zval *val;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &val) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for blob vbs_pack(array $values)");
	}

	vbs_packer_init(&pk, smart_write, &buf, -1);
	vbs::v_pack(&pk, val TSRMLS_CC);

	ZVAL_STRINGL(return_value, buf.c, buf.len, 1);																  
	smart_str_free(&buf);
}

PHP_FUNCTION(vbs_unpack)
{
	zval *z = NULL;
	long offset = 0;
	long num = 0;
	zval *used = NULL;
	zval *r = NULL;
	
	vbs_unpacker_t uk;
	char *str = NULL;
	int slen = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zll|z", &z, &offset, &num, &used) == FAILURE)
	{
		zend_error(E_ERROR, "Wrong parameters for array vbs_unpack(blob $vbs, int $offset, int $num [, int& $used])");
	}
	else if (Z_TYPE_P(z) == IS_STRING)
	{
		str = Z_STRVAL_P(z);
		slen = Z_STRLEN_P(z);
	}
	else if (VALID_OBJECT(z, vbs::classEntry_Blob))
	{
		z = zend_read_property(vbs::classEntry_Blob, z, "s", sizeof("s") - 1, 0 TSRMLS_CC);
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
	if (!vbs::v_unpack(&uk, num, &r TSRMLS_CC))
	{
		goto error;
	}

	*return_value = *r;
	FREE_ZVAL(r);
	if (used)
		ZVAL_LONG(used, uk.cur - uk.buf);
	return;

error:
	if (used)
			ZVAL_LONG(used, 0);
	ZVAL_NULL(return_value);
}

PHP_FUNCTION(dlog)
{
	char *identity = NULL, *tag = NULL, *content = NULL;
	int ilen = 0, tlen = 0, clen = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss",
			&identity, &ilen, &tag, &tlen, &content, &clen) == FAILURE)
	{
		// Do nothing
	}

	xstr_t tag_xs = XSTR_INIT((unsigned char *)tag, tlen);

	xstr_t content_xs = XSTR_INIT((unsigned char *)content, clen);

	xstr_t identity_xs;
	unsigned char idbuf[64];
	if (ilen > 0)
	{
		int k = ilen + 4;
		if (k > sizeof(idbuf) - 1)
			k = sizeof(idbuf) - 1;
		memcpy(idbuf, "PHP:", 4);
		memcpy(idbuf + 4, identity, k - 4);
		xstr_init(&identity_xs, idbuf, k);
	}
	else
	{
		xstr_const(&identity_xs, "PHP");
	}

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

	zdlog(&identity_xs, &tag_xs, &locus_xs, &content_xs);
}

static void php_xic_init_globals(zend_xic_globals *xic_globals TSRMLS_DC)
{
	xic_globals->the_engine = NULL;
	xic_globals->the_self = NULL;
	xic_globals->the_self_id = NULL;
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
	zval* zv = XIC_G(the_engine);
	if (zv)
	{
		xic::EnginePtr engine = MyObject<xic::Engine>::get(zv TSRMLS_CC); 
		engine->finish();
		zval_ptr_dtor(&zv);
	}
	XIC_G(the_engine) = NULL;

	zval* self = XIC_G(the_self);
	if (self)
	{
		zval_ptr_dtor(&self);
	}
	XIC_G(the_self) = NULL;

	zval* self_id = XIC_G(the_self_id);
	if (self_id)
	{
		zval_ptr_dtor(&self_id);
	}
	XIC_G(the_self_id) = NULL;

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
