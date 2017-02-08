#ifndef UTIL_H_
#define UTIL_H_

#include "php.h"
#include "php_ini.h"
#include "zend_interfaces.h"
#include "zend_exceptions.h"
#include "xslib/XRefCount.h"
#include <stdarg.h>
#include <string>


#define VALID_OBJECT(ZV, CE) 				\
	(Z_TYPE_P((ZV)) == IS_OBJECT && Z_OBJCE_P((ZV)) == (CE))


ssize_t get_self_process_id(char *id, size_t size);
std::string get_self_process(const char *id);
std::string get_default_ctx();
void update_ctx_caller(zval *ctx);

zend_string *get_ClassName(zval *obj);

void raise_Exception(long code TSRMLS_DC, const char *format, ...);


void* _create_MyObject(zend_class_entry*, size_t TSRMLS_DC);
void* _extract_MyObject(zval* TSRMLS_DC);

template<typename T>
struct MyObject
{
	zend_object zobj;
	XPtr<T> ptr;
public:
	static MyObject<T> *create(zend_class_entry *ce TSRMLS_DC)
	{
		MyObject<T> *o = static_cast<MyObject<T>*>(_create_MyObject(ce, sizeof(MyObject<T>) TSRMLS_CC));
		return o;
	}

	static MyObject<T> *extract(zval *zv TSRMLS_DC)
	{
		return static_cast<MyObject<T>*>(_extract_MyObject(zv TSRMLS_CC));
	}

	static XPtr<T> get(zval *zv TSRMLS_DC)
	{
		MyObject<T> *o = extract(zv TSRMLS_CC);
		if (o)
			return o->ptr;
		return XPtr<T>();
	}
};


#endif
