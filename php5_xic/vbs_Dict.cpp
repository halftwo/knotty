#include "vbs_Dict.h"
#include "xslib/xsdef.h"

using namespace vbs;

namespace vbs
{
zend_class_entry *classEntry_Dict = 0;
};

static zend_object_handlers _handlers;


static void delete_Dict(void *p TSRMLS_DC)
{
	zend_objects_free_object_storage(static_cast<zend_object*>(p) TSRMLS_CC);
}

static zend_object_value new_Dict(zend_class_entry *ce TSRMLS_DC)
{
	zend_object *obj = static_cast<zend_object*>(ecalloc(1, sizeof(zend_object)));

	zend_object_std_init(obj, ce TSRMLS_CC);
	object_properties_init(obj, ce);

	zend_object_value retval;
	retval.handle = zend_objects_store_put(obj, NULL, delete_Dict, NULL TSRMLS_CC);
	retval.handlers = &_handlers;

	return retval;
}

PHP_METHOD(vbs_Dict, __construct)
{
	raise_Exception(0 TSRMLS_CC, "v_Dict cannot be instantiated, use vbs_dict() function instead.");
	return;
}

PHP_METHOD(vbs_Dict, __toString)
{
	zval *zv = zend_read_property(classEntry_Dict, getThis(), "a", sizeof("a") - 1, 0 TSRMLS_CC);
	int num = 0;
	if (zv && Z_TYPE_P(zv) == IS_ARRAY)
	{
		HashTable *ht = Z_ARRVAL_P(zv);
		if (ht)
			num = zend_hash_num_elements(ht);
	}

	char buf[32];
	int len = snprintf(buf, sizeof(buf), "vbs_Dict(%d)", num);
	ZVAL_STRINGL(return_value, buf, len, 1);
}

PHP_METHOD(vbs_Dict, toArray)
{
	zval *zv = zend_read_property(classEntry_Dict, getThis(), "a", sizeof("a") - 1, 0 TSRMLS_CC);
	if (zv && Z_TYPE_P(zv) == IS_ARRAY)
	{
		ZVAL_ZVAL(return_value, zv, 1, 0);
	}
	else
	{
		array_init(return_value);
	}
}


static zend_function_entry _methods[] = {
	PHP_ME(vbs_Dict, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Dict, __toString, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Dict, toArray, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace vbs
{

bool init_Dict(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "v_Dict", _methods);
	classEntry_Dict = zend_register_internal_class(&ce TSRMLS_CC);
	classEntry_Dict->create_object = new_Dict;
	classEntry_Dict->ce_flags |= ZEND_ACC_FINAL_CLASS;

	_handlers = *zend_get_std_object_handlers();
	zend_declare_property_null(classEntry_Dict, "a", sizeof("a") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);

	return true;
}

bool create_Dict(zval* zv, zval* arr TSRMLS_DC)
{
	object_init_ex(zv, classEntry_Dict);
	zend_update_property(classEntry_Dict, zv, "a", sizeof("a") - 1, arr TSRMLS_CC);
	return true;
}


}

