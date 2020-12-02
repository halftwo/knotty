#include "xs_XError.h"
#include "util.h"
#include "xslib/xsdef.h"
#include "xslib/cxxstr.h"

/* XError is derived from Exception.
   XError adds a string field named "tag",
   and adds a method named getExname().
   Constructor of XError has a prototype of
   	XError(string $message, long $code, string $tag, Exception $previous);
 */

using namespace xs;

namespace xs
{
zend_class_entry *classEntry_XError = 0;
};


PHP_METHOD(xs_XError, __construct)
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
		zend_error(E_ERROR, "Wrong parameters for XError([string $message [, long $code [, string $tag [, Exception $previous = NULL]]]])");
	}

	obj = getThis();

	if (msg && msg_len > 0)
		zend_update_property_stringl(default_except_ce, MY_Z_OBJ_P(obj), "message", sizeof("message") - 1, msg, msg_len TSRMLS_CC);

	if (code)
		zend_update_property_long(default_except_ce, MY_Z_OBJ_P(obj), "code", sizeof("code") - 1, code TSRMLS_CC);

	if (prev)
		zend_update_property(default_except_ce, MY_Z_OBJ_P(obj), "previous", sizeof("previous") - 1, prev TSRMLS_CC);

	if (tag && tag_len > 0)
		zend_update_property_stringl(classEntry_XError, MY_Z_OBJ_P(obj), "tag", sizeof("tag") - 1, tag, tag_len TSRMLS_CC);

	return;
}

PHP_METHOD(xs_XError, getTag)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval* z = zend_read_property(classEntry_XError, MY_Z_OBJ_P(getThis()), "tag", sizeof("tag") - 1, 1, &rv TSRMLS_CC);
	if (z)
	{
		RETURN_ZVAL(z, 1, 0);
	}
	else
	{
		RETURN_STRING("");
	}
}

PHP_METHOD(xs_XError, getExname)
{
	zend_string* class_name = get_ClassName(getThis());

	if (class_name)
	{
		RETURN_STR(class_name);
	}
	else
	{
		RETURN_STRING("");
	}
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, code)
	ZEND_ARG_INFO(0, tag)
	ZEND_ARG_INFO(0, prev)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getTag, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getExname, 0, 0, 0)
ZEND_END_ARG_INFO()

static zend_function_entry _methods[] = {
	PHP_ME(xs_XError, __construct, arginfo_construct, ZEND_ACC_PUBLIC)
	PHP_ME(xs_XError, getTag, arginfo_getTag, ZEND_ACC_PUBLIC)
	PHP_ME(xs_XError, getExname, arginfo_getExname, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace xs
{

bool init_XError(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "XError", _methods);
	classEntry_XError = zend_register_internal_class_ex(&ce, zend_exception_get_default(TSRMLS_C));
	zend_declare_property_string(classEntry_XError, "tag", sizeof("tag") - 1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	return true;
}

}

