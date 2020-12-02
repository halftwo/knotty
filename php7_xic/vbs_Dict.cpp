#include "vbs_Dict.h"
#include "xslib/xsdef.h"

using namespace vbs;

namespace vbs
{
zend_class_entry *classEntry_Dict = 0;
};

static zend_object_handlers _handlers;


static void delete_Dict(zend_object *p TSRMLS_DC)
{
	// TODO?
}

static zend_object* new_Dict(zend_class_entry *ce TSRMLS_DC)
{
	zend_object *obj = static_cast<zend_object*>(ecalloc(1, sizeof(zend_object)));

	zend_object_std_init(obj, ce TSRMLS_CC);
	object_properties_init(obj, ce);

	obj->handlers = &_handlers;
	_handlers.free_obj = delete_Dict;

	return obj;
}

PHP_METHOD(vbs_Dict, __construct)
{
	raise_Exception(0 TSRMLS_CC, "v_Dict cannot be instantiated, use vbs_dict() function instead.");
	return;
}

PHP_METHOD(vbs_Dict, __toString)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval *zv = zend_read_property(classEntry_Dict, MY_Z_OBJ_P(getThis()), "a", sizeof("a") - 1, 0, &rv TSRMLS_CC);
	int num = 0;
	if (zv && Z_TYPE_P(zv) == IS_ARRAY)
	{
		HashTable *ht = Z_ARRVAL_P(zv);
		if (ht)
			num = zend_hash_num_elements(ht);
	}

	char buf[32];
	int len = snprintf(buf, sizeof(buf), "v_Dict(%d)", num);
	RETURN_STRINGL(buf, len);
}

PHP_METHOD(vbs_Dict, toArray)
{
	zval rv;
	ZVAL_UNDEF(&rv);
	zval *zv = zend_read_property(classEntry_Dict, MY_Z_OBJ_P(getThis()), "a", sizeof("a") - 1, 0, &rv TSRMLS_CC);
	if (zv && Z_TYPE_P(zv) == IS_ARRAY)
	{
		RETURN_ZVAL(zv, 1, 0);
	}
	else
	{
		array_init(return_value);
	}
}


ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_toString, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_toArray, 0, 0, 0)
ZEND_END_ARG_INFO()

static zend_function_entry _methods[] = {
	PHP_ME(vbs_Dict, __construct, arginfo_construct, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Dict, __toString, arginfo_toString, ZEND_ACC_PUBLIC)
	PHP_ME(vbs_Dict, toArray, arginfo_toArray, ZEND_ACC_PUBLIC)
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
	classEntry_Dict->ce_flags |= ZEND_ACC_FINAL;

	_handlers = *zend_get_std_object_handlers();
	zend_declare_property_null(classEntry_Dict, "a", sizeof("a") - 1, ZEND_ACC_PROTECTED TSRMLS_CC);

	return true;
}

bool create_Dict(zval* zv, zval* arr TSRMLS_DC)
{
	object_init_ex(zv, classEntry_Dict);
	zend_update_property(classEntry_Dict, MY_Z_OBJ_P(zv), "a", sizeof("a") - 1, arr TSRMLS_CC);
	return true;
}


}

