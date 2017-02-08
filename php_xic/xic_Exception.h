#ifndef XIC_EXCEPTION_H_
#define XIC_EXCEPTION_H_

#include "php_xic.h"
#include <stdio.h>
#include <string>
#include <map>


namespace xic
{

extern zend_class_entry *classEntry_Exception;

bool init_Exception(TSRMLS_D);

void raise_LocalException(const std::string& proxy, const std::exception& ex TSRMLS_DC);
void raise_RemoteException(const std::string& proxy, zval* detail TSRMLS_DC);



};

#endif
