#include "xic_Exception.h"
#include "xs_XError.h"
#include "util.h"
#include "xslib/xsdef.h"
#include "xslib/cxxstr.h"

/* xic_Exception is derived from XError, which is derived from Exception
 */

using namespace xic;

namespace xic
{
zend_class_entry *classEntry_Exception = 0;
};


PHP_METHOD(xic_Exception, __construct)
{
	char *msg = NULL, *tag = NULL;
	int msg_len = 0, tag_len = 0;
	long code = 0;
	zval *prev = NULL;
	zval *obj;
	zend_class_entry* default_except_ce = zend_exception_get_default(TSRMLS_C);

	if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS() TSRMLS_CC, "|slsO!",
		&msg, &msg_len, &code, &tag, &tag_len, &prev, default_except_ce) == FAILURE)
	{
		 zend_error(E_ERROR, "Wrong parameters for xic_Exception([string $message [, long $code [, string $tag [, Exception $previous = NULL]]]])");
	}

	obj = getThis();

	if (msg && msg_len > 0)
		zend_update_property_stringl(default_except_ce, obj, "message", sizeof("message") - 1, msg, msg_len TSRMLS_CC);

	if (code)
		zend_update_property_long(default_except_ce, obj, "code", sizeof("code") - 1, code TSRMLS_CC);

	if (prev)
		zend_update_property(default_except_ce, obj, "previous", sizeof("previous") - 1, prev TSRMLS_CC);

	if (tag && tag_len > 0)
		zend_update_property_stringl(xs::classEntry_XError, obj, "tag", sizeof("tag") - 1, tag, tag_len TSRMLS_CC);

	zend_update_property_bool(classEntry_Exception, obj, "_local", sizeof("_local") - 1, true TSRMLS_CC);

	return;
}

PHP_METHOD(xic_Exception, isLocal)
{
	zval* z = zend_read_property(classEntry_Exception, getThis(), "_local", sizeof("_local") - 1, 1 TSRMLS_CC);
	RETURN_ZVAL(z, 1, 0);
}

PHP_METHOD(xic_Exception, getProxyString)
{
	zval* z = zend_read_property(classEntry_Exception, getThis(), "proxy", sizeof("proxy") - 1, 1 TSRMLS_CC);
	RETURN_ZVAL(z, 1, 0);
}

PHP_METHOD(xic_Exception, getExname)
{
	zval* obj = getThis();
	zval* z = zend_read_property(classEntry_Exception, obj, "exname", sizeof("exname") - 1, 1 TSRMLS_CC);
	if (z && Z_TYPE_P(z) == IS_STRING && Z_STRLEN_P(z) > 0)
	{
		RETURN_ZVAL(z, 1, 0);
	}
	else
	{
		int clen = 0;
		char *class_name = get_ClassName(obj, &clen TSRMLS_CC);

		if (class_name)
		{
			RETURN_STRINGL(class_name, clen, 0);
		}
		else
		{
			RETURN_STRING("", 1);
		}
	}
}

PHP_METHOD(xic_Exception, getRaiser)
{
	zval* z = zend_read_property(classEntry_Exception, getThis(), "raiser", sizeof("raiser") - 1, 1 TSRMLS_CC);
	RETURN_ZVAL(z, 1, 0);
}

PHP_METHOD(xic_Exception, getDetail)
{
	zval* z = zend_read_property(classEntry_Exception, getThis(), "detail", sizeof("detail") - 1, 1 TSRMLS_CC);
	RETURN_ZVAL(z, 1, 0);
}

static zend_function_entry _methods[] = {
	PHP_ME(xic_Exception, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, isLocal, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getProxyString, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getExname, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getRaiser, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getDetail, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace xic
{

bool init_Exception(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "xic_Exception", _methods);
	classEntry_Exception = zend_register_internal_class_ex(&ce, xs::classEntry_XError, NULL TSRMLS_CC);

	zend_declare_property_bool(classEntry_Exception, "_local", sizeof("_local") - 1, false, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(classEntry_Exception, "proxy", sizeof("proxy") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(classEntry_Exception, "exname", sizeof("exname") - 1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(classEntry_Exception, "raiser", sizeof("raiser") - 1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(classEntry_Exception, "detail", sizeof("detail") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);

	return true;
}

void raise_LocalException(const std::string& proxy, const std::exception& ex TSRMLS_DC)
{
	zval* zv;
	MAKE_STD_ZVAL(zv);
	object_init_ex(zv, classEntry_Exception);

	zend_update_property_stringl(classEntry_Exception, zv, "proxy", sizeof("proxy") - 1, (char *)proxy.data(), proxy.length() TSRMLS_CC);
	zend_update_property_bool(classEntry_Exception, zv, "_local", sizeof("_local") - 1, true TSRMLS_CC);

	const XError *e = dynamic_cast<const XError*>(&ex);
	std::string exname = e ? e->exname() : demangle_cxxname(typeid(ex).name());

	zend_update_property_stringl(classEntry_Exception, zv, "exname", sizeof("exname") - 1, (char *)exname.data(), exname.length() TSRMLS_CC);
	zend_update_property_string(classEntry_Exception, zv, "message", sizeof("message") - 1, (char *)ex.what() TSRMLS_CC);

	if (e)
	{
		if (e->code())
			zend_update_property_long(classEntry_Exception, zv, "code", sizeof("code") - 1, e->code() TSRMLS_CC);
		zend_update_property_stringl(classEntry_Exception, zv, "tag", sizeof("tag") - 1, (char *)e->tag().data(), e->tag().length() TSRMLS_CC);
	}
	else
	{
		zend_update_property_string(classEntry_Exception, zv, "tag", sizeof("tag") - 1, "" TSRMLS_CC);
	}

	zval* detail;
	MAKE_STD_ZVAL(detail);
	array_init(detail);
	add_assoc_string(detail, "what", (char *)ex.what(), 1);
	if (e)
	{
		add_assoc_string(detail, "file", (char *)e->file(), 1);
		add_assoc_long(detail, "line", e->line());
		add_assoc_stringl(detail, "calltrace", (char *)e->calltrace().data(), e->calltrace().length(), 1);
	}
	zend_update_property(classEntry_Exception, zv, "detail", sizeof("detail") - 1, detail TSRMLS_CC);
	ZVAL_DELREF(detail);

	zend_throw_exception_object(zv TSRMLS_CC);
}

void raise_RemoteException(const std::string& proxy, zval* info TSRMLS_DC)
{
	zval* zv;
	MAKE_STD_ZVAL(zv);
	object_init_ex(zv, classEntry_Exception);

	zend_update_property_stringl(classEntry_Exception, zv, "proxy", sizeof("proxy") - 1, (char *)proxy.data(), proxy.length() TSRMLS_CC);

	HashTable *ht = HASH_OF(info);
	long code = 0;
	const char *tag = "";
	const char *message = "";
	const char *exname = "UNKNOWN_EXCEPTION";
	const char *raiser = "UNKNOWN_RAISER";
	zval *detail = NULL;
	if (ht)
	{
		zval **tmp;
		if (zend_symtable_find(ht, "exname", sizeof("exname"), (void**)&tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_STRING)
		{
			exname = Z_STRVAL_PP(tmp);
			if (exname[0])
				zend_update_property_string(classEntry_Exception, zv, "exname", sizeof("exname") - 1, (char*)exname TSRMLS_CC);
		}

		if (zend_symtable_find(ht, "raiser", sizeof("raiser"), (void**)&tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_STRING)
		{
			raiser = Z_STRVAL_PP(tmp);
			if (raiser[0])
				zend_update_property_string(classEntry_Exception, zv, "raiser", sizeof("raiser") - 1, (char*)raiser TSRMLS_CC);
		}

		if (zend_symtable_find(ht, "code", sizeof("code"), (void**)&tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_LONG)
		{
			code = Z_LVAL_PP(tmp);
			if (code)
				zend_update_property_long(classEntry_Exception, zv, "code", sizeof("code") - 1, code TSRMLS_CC);
		}

		if (zend_symtable_find(ht, "tag", sizeof("tag"), (void**)&tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_STRING)
		{
			tag = Z_STRVAL_PP(tmp);
			if (tag[0])
				zend_update_property_string(classEntry_Exception, zv, "tag", sizeof("tag") - 1, (char*)tag TSRMLS_CC);
		}

		if (zend_symtable_find(ht, "message", sizeof("message"), (void**)&tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_STRING)
		{
			message = Z_STRVAL_PP(tmp);
		}

		if (zend_symtable_find(ht, "detail", sizeof("detail"), (void**)&tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_ARRAY)
		{
			detail = *tmp;
		}
	}

	char buf[4096];
	size_t len = 0;
	if (tag[0])
		len = snprintf(buf, sizeof(buf), "!%s(%ld,%s) on %s", exname, code, tag, raiser);
	else
		len = snprintf(buf, sizeof(buf), "!%s(%ld) on %s", exname, code, raiser);
	
	if (message[0] && len < sizeof(buf))
		len += snprintf(buf + len, sizeof(buf) - len, " --- %s", message);

	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;

	zend_update_property_stringl(classEntry_Exception, zv, "message", sizeof("message") - 1, buf, len TSRMLS_CC);

	if (detail)
	{
		zend_update_property(classEntry_Exception, zv, "detail", sizeof("detail") - 1, detail TSRMLS_CC);
	}
	else
	{
		MAKE_STD_ZVAL(detail);
		array_init(detail);
		zend_update_property(classEntry_Exception, zv, "detail", sizeof("detail") - 1, detail TSRMLS_CC);
		ZVAL_DELREF(detail);
	}

	zend_throw_exception_object(zv TSRMLS_CC);
}


}

