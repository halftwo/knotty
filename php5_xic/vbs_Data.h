#ifndef vbs_Data_h_
#define vbs_Data_h_

#include "php_xic.h"


namespace vbs
{

extern zend_class_entry *classEntry_Data;

bool init_Data(TSRMLS_D);

bool create_Data(zval* zv, zval* dat, long descriptor TSRMLS_DC);


};

#endif
