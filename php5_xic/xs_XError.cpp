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
	int msg_len = 0, tag_len = 0;
	long code = 0;
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
		zend_update_property_stringl(default_except_ce, obj, "message", sizeof("message") - 1, msg, msg_len TSRMLS_CC);

	if (code)
		zend_update_property_long(default_except_ce, obj, "code", sizeof("code") - 1, code TSRMLS_CC);

	if (prev)
		zend_update_property(default_except_ce, obj, "previous", sizeof("previous") - 1, prev TSRMLS_CC);

	if (tag && tag_len > 0)
		zend_update_property_stringl(classEntry_XError, obj, "tag", sizeof("tag") - 1, tag, tag_len TSRMLS_CC);

	return;
}

PHP_METHOD(xs_XError, getTag)
{
	zval* z = zend_read_property(classEntry_XError, getThis(), "tag", sizeof("tag") - 1, 1 TSRMLS_CC);
	RETURN_ZVAL(z, 1, 0);
}

PHP_METHOD(xs_XError, getExname)
{
	int clen = 0;
	char *class_name = get_ClassName(getThis(), &clen TSRMLS_CC);

	if (class_name)
	{
		RETURN_STRINGL(class_name, clen, 0);
	}
	else
	{
		RETURN_STRING("", 1);
	}
}

static zend_function_entry _methods[] = {
	PHP_ME(xs_XError, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xs_XError, getTag, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xs_XError, getExname, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace xs
{

bool init_XError(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "XError", _methods);
	classEntry_XError = zend_register_internal_class_ex(&ce, zend_exception_get_default(TSRMLS_C), NULL TSRMLS_CC);
	zend_declare_property_string(classEntry_XError, "tag", sizeof("tag") - 1, "", ZEND_ACC_PROTECTED TSRMLS_CC);
	return true;
}

}

