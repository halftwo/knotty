#include "xslib/xsdef.h"
#include "vbs_Decimal.h"
#include "xslib/decimal64.h"

using namespace vbs;

namespace vbs
{
zend_class_entry *classEntry_Decimal = 0;
};

static zend_object_handlers _handlers;


static void delete_Decimal(zend_object *p TSRMLS_DC)
{
	// TODO ?
}

static zend_object* new_Decimal(zend_class_entry *ce TSRMLS_DC)
{
	zend_object *obj = static_cast<zend_object*>(ecalloc(1, sizeof(zend_object)));

	zend_object_std_init(obj, ce TSRMLS_CC);
	object_properties_init(obj, ce);

	obj->handlers = &_handlers;
	_handlers.free_obj = delete_Decimal;

	return obj;
}

PHP_METHOD(vbs_Decimal, __construct)
{
	raise_Exception(0 TSRMLS_CC, "v_Decimal cannot be instantiated, use vbs_decimal() function instead.");
	return;
}

PHP_METHOD(vbs_Decimal, __toString)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval *zv = zend_read_property(classEntry_Decimal, getThis(), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
	RETURN_ZVAL(zv, 1, 0);
}

PHP_METHOD(vbs_Decimal, toString)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval *zv = zend_read_property(classEntry_Decimal, getThis(), "s", sizeof("s") - 1, 0, &rv TSRMLS_CC);
	RETURN_ZVAL(zv, 1, 0);
}


static zend_function_entry _methods[] = {
	PHP_ME(vbs_Decimal, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Decimal, __toString, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Decimal, toString, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace vbs
{

bool init_Decimal(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "v_Decimal", _methods);
	classEntry_Decimal = zend_register_internal_class(&ce TSRMLS_CC);
	classEntry_Decimal->create_object = new_Decimal;
	classEntry_Decimal->ce_flags |= ZEND_ACC_FINAL;

	_handlers = *zend_get_std_object_handlers();
	zend_declare_property_null(classEntry_Decimal, "s", sizeof("s") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);

	return true;
}

void create_DecimalNoCheck(zval* obj, zval* zv TSRMLS_DC)
{
	assert(Z_TYPE_P(zv) == IS_STRING);
	object_init_ex(obj, classEntry_Decimal);
	zend_update_property(classEntry_Decimal, obj, "s", sizeof("s") - 1, zv TSRMLS_CC);
}

bool create_Decimal(zval* obj, zval* zv TSRMLS_DC)
{
	char buf[32], *end;
	decimal64_t dec;

	object_init_ex(obj, classEntry_Decimal);
	if (Z_TYPE_P(zv) == IS_STRING)
	{
		decimal64_from_cstr(&dec, Z_STRVAL_P(zv), &end);
		if (!(*end==0 || ((*end == 'D' || *end == 'd') && end[1]==0)))
			throw XERROR_FMT(XError, "Invalid decimal string \"%s\"", Z_STRVAL_P(zv));
	}
	else
	{
		if (Z_TYPE_P(zv) == IS_LONG)
		{
			snprintf(buf, sizeof(buf), "%ld", Z_LVAL_P(zv));
		}
		else if (Z_TYPE_P(zv) == IS_DOUBLE)
		{
			snprintf(buf, sizeof(buf), "%.16G", Z_DVAL_P(zv));
		}
		else
		{
			assert(!"Can't reach here!");
		}

		decimal64_from_cstr(&dec, buf, &end);
	}

	int len = decimal64_to_cstr(dec, buf);
	zval z;
	ZVAL_UNDEF(&z);
	ZVAL_STRINGL(&z, buf, len);
	zend_update_property(classEntry_Decimal, obj, "s", sizeof("s") - 1, &z TSRMLS_CC);
	Z_DELREF_P(&z);
	return true;
}


}

