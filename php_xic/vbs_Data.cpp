#include "vbs_Data.h"
#include "xslib/vbs_pack.h"
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
	zval *zv = zend_read_property(classEntry_Data, MY_Z_OBJ_P(getThis()), "d", sizeof("d") - 1, 0, &rv TSRMLS_CC);
	RETURN_ZVAL(zv, 1, 0);
}

PHP_METHOD(vbs_Data, getDescriptor)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval *zv = zend_read_property(classEntry_Data, MY_Z_OBJ_P(getThis()), "r", sizeof("r") - 1, 0, &rv TSRMLS_CC);
	RETURN_ZVAL(zv, 1, 0);
}


ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getData, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getDescriptor, 0, 0, 0)
ZEND_END_ARG_INFO()

static zend_function_entry _methods[] = {
	PHP_ME(vbs_Data, __construct, arginfo_construct, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Data, getData, arginfo_getData, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Data, getDescriptor, arginfo_getDescriptor, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace vbs
{

bool init_Data(TSRMLS_D)
{
	zend_class_entry ce;

	REGISTER_MAIN_LONG_CONSTANT("VBS_SPECIAL_DESCRIPTOR", VBS_SPECIAL_DESCRIPTOR, CONST_CS | CONST_PERSISTENT);
	REGISTER_MAIN_LONG_CONSTANT("VBS_DESCRIPTOR_MAX", VBS_DESCRIPTOR_MAX, CONST_CS | CONST_PERSISTENT);

	INIT_CLASS_ENTRY(ce, "v_Data", _methods);
	classEntry_Data = zend_register_internal_class(&ce TSRMLS_CC);
	classEntry_Data->create_object = new_Data;
	classEntry_Data->ce_flags |= ZEND_ACC_FINAL;

	_handlers = *zend_get_std_object_handlers();
	zend_declare_property_long(classEntry_Data, "r", sizeof("r") - 1, 0, ZEND_ACC_PROTECTED TSRMLS_CC);
	return true;
}

bool create_Data(zval* obj, zval* dat, int descriptor TSRMLS_DC)
{
	object_init_ex(obj, classEntry_Data);

	add_property_zval(obj, "d", dat);
	zend_update_property_long(classEntry_Data, MY_Z_OBJ_P(obj), "r", sizeof("r") - 1, descriptor);
	return true;
}

}

