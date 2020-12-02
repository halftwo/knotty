#ifndef xs_XError_h_
#define xs_XError_h_

#include "php_xic.h"
#include <stdio.h>
#include <string>
#include <map>


namespace xs
{

extern zend_class_entry *classEntry_XError;

bool init_XError(TSRMLS_D);


};

#endif
