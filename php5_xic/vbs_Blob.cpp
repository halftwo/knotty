#include "vbs_Blob.h"
#include "xslib/xsdef.h"

using namespace vbs;

namespace vbs
{
zend_class_entry *classEntry_Blob = 0;
};

static zend_object_handlers _handlers;


static void delete_Blob(void *p TSRMLS_DC)
{
	zend_objects_free_object_storage(static_cast<zend_object*>(p) TSRMLS_CC);
}

static zend_object_value new_Blob(zend_class_entry *ce TSRMLS_DC)
{
	zend_object *obj = static_cast<zend_object*>(ecalloc(1, sizeof(zend_object)));

	zend_object_std_init(obj, ce TSRMLS_CC);
	object_properties_init(obj, ce);

	zend_object_value retval;
	retval.handle = zend_objects_store_put(obj, NULL, delete_Blob, NULL TSRMLS_CC);
	retval.handlers = &_handlers;

	return retval;
}

PHP_METHOD(vbs_Blob, __construct)
{
	raise_Exception(0 TSRMLS_CC, "v_Blob cannot be instantiated, use vbs_blob() function instead.");
	return;
}

PHP_METHOD(vbs_Blob, __toString)
{
	zval *zv = zend_read_property(classEntry_Blob, getThis(), "s", sizeof("s") - 1, 0 TSRMLS_CC);
	ZVAL_ZVAL(return_value, zv, 1, 0);
}

PHP_METHOD(vbs_Blob, toString)
{
	zval *zv = zend_read_property(classEntry_Blob, getThis(), "s", sizeof("s") - 1, 0 TSRMLS_CC);
	ZVAL_ZVAL(return_value, zv, 1, 0);
}


static zend_function_entry _methods[] = {
	PHP_ME(vbs_Blob, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Blob, __toString, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Blob, toString, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace vbs
{

bool init_Blob(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "v_Blob", _methods);
	classEntry_Blob = zend_register_internal_class(&ce TSRMLS_CC);
	classEntry_Blob->create_object = new_Blob;
	classEntry_Blob->ce_flags |= ZEND_ACC_FINAL_CLASS;

	_handlers = *zend_get_std_object_handlers();
	zend_declare_property_null(classEntry_Blob, "s", sizeof("s") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);

	return true;
}

bool create_Blob(zval* zv, zval* str TSRMLS_DC)
{
	object_init_ex(zv, classEntry_Blob);
	zend_update_property(classEntry_Blob, zv, "s", sizeof("s") - 1, str TSRMLS_CC);
	return true;
}


}

