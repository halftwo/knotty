#ifndef vbs_Dict_h_
#define vbs_Dict_h_

#include "php_xic.h"


namespace vbs
{

extern zend_class_entry *classEntry_Dict;

bool init_Dict(TSRMLS_D);

bool create_Dict(zval* , zval* arr TSRMLS_DC);


};

#endif
