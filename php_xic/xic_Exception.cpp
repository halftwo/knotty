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
	size_t msg_len = 0, tag_len = 0;
	zend_long code = 0;
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
		zend_update_property_stringl(default_except_ce, MY_Z_OBJ_P(obj), "message", sizeof("message") - 1, msg, msg_len TSRMLS_CC);

	if (code)
		zend_update_property_long(default_except_ce, MY_Z_OBJ_P(obj), "code", sizeof("code") - 1, code TSRMLS_CC);

	if (prev)
		zend_update_property(default_except_ce, MY_Z_OBJ_P(obj), "previous", sizeof("previous") - 1, prev TSRMLS_CC);

	if (tag && tag_len > 0)
		zend_update_property_stringl(xs::classEntry_XError, MY_Z_OBJ_P(obj), "tag", sizeof("tag") - 1, tag, tag_len TSRMLS_CC);

	zend_update_property_bool(classEntry_Exception, MY_Z_OBJ_P(obj), "_local", sizeof("_local") - 1, true TSRMLS_CC);

	return;
}

PHP_METHOD(xic_Exception, isLocal)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval* z = zend_read_property(classEntry_Exception, MY_Z_OBJ_P(getThis()), "_local", sizeof("_local") - 1, 1, &rv TSRMLS_CC);
	if (z)
	{
		RETURN_ZVAL(z, 1, 0);
	}
	else
	{
		RETURN_BOOL(1);
	}
}

PHP_METHOD(xic_Exception, getProxyString)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval* z = zend_read_property(classEntry_Exception, MY_Z_OBJ_P(getThis()), "proxy", sizeof("proxy") - 1, 1, &rv TSRMLS_CC);
	if (z)
	{
		RETURN_ZVAL(z, 1, 0);
	}
	else
	{
		RETURN_STRING("");
	}
}

PHP_METHOD(xic_Exception, getExname)
{
	zval* obj = getThis();
	zval rv;
	ZVAL_UNDEF(&rv);
	zval* z = zend_read_property(classEntry_Exception, MY_Z_OBJ_P(obj), "exname", sizeof("exname") - 1, 1, &rv TSRMLS_CC);
	if (z && Z_TYPE_P(z) == IS_STRING && Z_STRLEN_P(z) > 0)
	{
		RETURN_ZVAL(z, 1, 0);
	}
	else
	{
		zend_string *class_name = get_ClassName(obj);
		if (class_name)
		{
			RETURN_STR(class_name);
		}
		else
		{
			RETURN_STRING("");
		}
	}
}

PHP_METHOD(xic_Exception, getRaiser)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval* z = zend_read_property(classEntry_Exception, MY_Z_OBJ_P(getThis()), "raiser", sizeof("raiser") - 1, 1, &rv TSRMLS_CC);
	if (z)
	{
		RETURN_ZVAL(z, 1, 0);
	}
	else
	{
		RETURN_STRING("UNKNOWN_RAISER");
	}
}

PHP_METHOD(xic_Exception, getDetail)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval* z = zend_read_property(classEntry_Exception, MY_Z_OBJ_P(getThis()), "detail", sizeof("detail") - 1, 1, &rv TSRMLS_CC);
	// What if z == NULL
	RETURN_ZVAL(z, 1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
	ZEND_ARG_INFO(0, tag)
	ZEND_ARG_INFO(0, prev)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_isLocal, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getProxyString, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getExname, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getRaiser, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getDetail, 0, 0, 0)
ZEND_END_ARG_INFO()

static zend_function_entry _methods[] = {
	PHP_ME(xic_Exception, __construct, arginfo_construct, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, isLocal, arginfo_isLocal, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getProxyString, arginfo_getProxyString, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getExname, arginfo_getExname, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getRaiser, arginfo_getRaiser, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Exception, getDetail, arginfo_getDetail, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace xic
{

bool init_Exception(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "xic_Exception", _methods);
	classEntry_Exception = zend_register_internal_class_ex(&ce, xs::classEntry_XError);

	zend_declare_property_bool(classEntry_Exception, "_local", sizeof("_local") - 1, false, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(classEntry_Exception, "proxy", sizeof("proxy") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(classEntry_Exception, "exname", sizeof("exname") - 1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_string(classEntry_Exception, "raiser", sizeof("raiser") - 1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	zend_declare_property_null(classEntry_Exception, "detail", sizeof("detail") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);

	return true;
}

void raise_LocalException(const std::string& proxy, const std::exception& ex TSRMLS_DC)
{
	zval zv;
	ZVAL_UNDEF(&zv);
	object_init_ex(&zv, classEntry_Exception);

	zend_update_property_stringl(classEntry_Exception, MY_Z_OBJ_P(&zv), "proxy", sizeof("proxy") - 1, (char *)proxy.data(), proxy.length() TSRMLS_CC);
	zend_update_property_bool(classEntry_Exception, MY_Z_OBJ_P(&zv), "_local", sizeof("_local") - 1, true TSRMLS_CC);

	const XError *e = dynamic_cast<const XError*>(&ex);
	std::string exname = e ? e->exname() : demangle_cxxname(typeid(ex).name());

	zend_update_property_stringl(classEntry_Exception, MY_Z_OBJ_P(&zv), "exname", sizeof("exname") - 1, (char *)exname.data(), exname.length() TSRMLS_CC);
	zend_update_property_string(classEntry_Exception, MY_Z_OBJ_P(&zv), "message", sizeof("message") - 1, (char *)ex.what() TSRMLS_CC);

	if (e)
	{
		if (e->code())
			zend_update_property_long(classEntry_Exception, MY_Z_OBJ_P(&zv), "code", sizeof("code") - 1, e->code() TSRMLS_CC);
		zend_update_property_stringl(classEntry_Exception, MY_Z_OBJ_P(&zv), "tag", sizeof("tag") - 1, (char *)e->tag().data(), e->tag().length() TSRMLS_CC);
	}
	else
	{
		zend_update_property_string(classEntry_Exception, MY_Z_OBJ_P(&zv), "tag", sizeof("tag") - 1, "" TSRMLS_CC);
	}

	zval detail;
	ZVAL_UNDEF(&detail);
	array_init(&detail);
	add_assoc_string_ex(&detail, "what", sizeof("what") - 1, (char *)ex.what());
	if (e)
	{
		add_assoc_string_ex(&detail, "file", sizeof("file") - 1, (char *)e->file());
		add_assoc_long_ex(&detail, "line", sizeof("line") - 1, e->line());
		add_assoc_stringl_ex(&detail, "calltrace", sizeof("calltrace") - 1, (char *)e->calltrace().data(), e->calltrace().length());
	}
	zend_update_property(classEntry_Exception, MY_Z_OBJ_P(&zv), "detail", sizeof("detail") - 1, &detail TSRMLS_CC);

	zend_throw_exception_object(&zv TSRMLS_CC);
}

void raise_RemoteException(const std::string& proxy, zval* info TSRMLS_DC)
{
	zval zv;
	ZVAL_UNDEF(&zv);
	object_init_ex(&zv, classEntry_Exception);

	zend_update_property_stringl(classEntry_Exception, MY_Z_OBJ_P(&zv), "proxy", sizeof("proxy") - 1, (char *)proxy.data(), proxy.length() TSRMLS_CC);

	HashTable *ht = HASH_OF(info);
	long code = 0;
	const char *tag = "";
	const char *message = "";
	const char *exname = "UNKNOWN_EXCEPTION";
	const char *raiser = "UNKNOWN_RAISER";
	zval detail;
	ZVAL_UNDEF(&detail);
	if (ht)
	{
		zval *tmp;
		tmp = zend_symtable_str_find(ht, "exname", sizeof("exname") - 1);
		if (tmp && Z_TYPE_P(tmp) == IS_STRING)
		{
			exname = Z_STRVAL_P(tmp);
			if (exname[0])
				zend_update_property_string(classEntry_Exception, MY_Z_OBJ_P(&zv), "exname", sizeof("exname") - 1, exname TSRMLS_CC);
		}

		tmp = zend_symtable_str_find(ht, "raiser", sizeof("raiser") - 1);
		if (tmp && Z_TYPE_P(tmp) == IS_STRING)
		{
			raiser = Z_STRVAL_P(tmp);
			if (raiser[0])
				zend_update_property_string(classEntry_Exception, MY_Z_OBJ_P(&zv), "raiser", sizeof("raiser") - 1, raiser TSRMLS_CC);
		}

		tmp = zend_symtable_str_find(ht, "code", sizeof("code") - 1);
		if (tmp && Z_TYPE_P(tmp) == IS_LONG)
		{
			code = Z_LVAL_P(tmp);
			if (code)
				zend_update_property_long(classEntry_Exception, MY_Z_OBJ_P(&zv), "code", sizeof("code") - 1, code TSRMLS_CC);
		}

		tmp = zend_symtable_str_find(ht, "tag", sizeof("tag") - 1);
		if (tmp && Z_TYPE_P(tmp) == IS_STRING)
		{
			tag = Z_STRVAL_P(tmp);
			if (tag[0])
				zend_update_property_string(classEntry_Exception, MY_Z_OBJ_P(&zv), "tag", sizeof("tag") - 1, tag TSRMLS_CC);
		}

		tmp = zend_symtable_str_find(ht, "message", sizeof("message") - 1);
		if (tmp && Z_TYPE_P(tmp) == IS_STRING)
		{
			message = Z_STRVAL_P(tmp);
		}

		tmp = zend_symtable_str_find(ht, "detail", sizeof("detail") - 1);
		if (tmp && Z_TYPE_P(tmp) == IS_ARRAY)
		{
			ZVAL_COPY_VALUE(&detail, tmp);
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

	zend_update_property_stringl(classEntry_Exception, MY_Z_OBJ_P(&zv), "message", sizeof("message") - 1, buf, len TSRMLS_CC);

	if (!Z_ISUNDEF(detail))
	{
		zend_update_property(classEntry_Exception, MY_Z_OBJ_P(&zv), "detail", sizeof("detail") - 1, &detail TSRMLS_CC);
	}
	else
	{
		array_init(&detail);
		zend_update_property(classEntry_Exception, MY_Z_OBJ_P(&zv), "detail", sizeof("detail") - 1, &detail TSRMLS_CC);
	}

	zend_throw_exception_object(&zv TSRMLS_CC);
}


}

