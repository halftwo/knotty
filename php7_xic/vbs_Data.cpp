#include "vbs_Data.h"
#include "xslib/xsdef.h"

using namespace vbs;

namespace vbs
{
zend_class_entry *classEntry_Data = 0;
};

static zend_object_handlers _handlers;


static void delete_Data(zend_object *p TSRMLS_DC)
{
	// TODO ?
}

static zend_object* new_Data(zend_class_entry *ce TSRMLS_DC)
{
	zend_object *obj = static_cast<zend_object*>(ecalloc(1, sizeof(zend_object)));

	zend_object_std_init(obj, ce TSRMLS_CC);
	object_properties_init(obj, ce);

	obj->handlers = &_handlers;
	_handlers.free_obj = delete_Data;

	return obj;
}

PHP_METHOD(vbs_Data, __construct)
{
	raise_Exception(0 TSRMLS_CC, "v_Data cannot be instantiated, use vbs_data() function instead.");
	return;
}

PHP_METHOD(vbs_Data, getData)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval *zv = zend_read_property(classEntry_Data, getThis(), "d", sizeof("d") - 1, 0, &rv TSRMLS_CC);
	RETURN_ZVAL(zv, 1, 0);
}

PHP_METHOD(vbs_Data, getDescriptor)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval *zv = zend_read_property(classEntry_Data, getThis(), "r", sizeof("r") - 1, 0, &rv TSRMLS_CC);
	RETURN_ZVAL(zv, 1, 0);
}

static zend_function_entry _methods[] = {
	PHP_ME(vbs_Data, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Data, getData, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Data, getDescriptor, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace vbs
{

bool init_Data(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "v_Data", _methods);
	classEntry_Data = zend_register_internal_class(&ce TSRMLS_CC);
	classEntry_Data->create_object = new_Data;
	classEntry_Data->ce_flags |= ZEND_ACC_FINAL;

	_handlers = *zend_get_std_object_handlers();
	zend_declare_property_null(classEntry_Data, "d", sizeof("d") - 1, ZEND_ACC_PUBLIC TSRMLS_CC);
//	zend_declare_property_long(classEntry_Data, "r", sizeof("r") - 1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);

	return true;
}

bool create_Data(zval* obj, zval* dat, int descriptor TSRMLS_DC)
{
	object_init_ex(obj, classEntry_Data);

	zend_update_property(classEntry_Data, obj, "d", sizeof("d") - 1, dat);
//	zend_update_property_long(classEntry_Data, obj, "r", sizeof("r") - 1, descriptor);
	return true;
}

}

