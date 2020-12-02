#ifndef vbs_Decimal_h_
#define vbs_Decimal_h_

#include "php_xic.h"


namespace vbs
{

extern zend_class_entry *classEntry_Decimal;

bool init_Decimal(TSRMLS_D);

bool create_Decimal(zval* zv, zval* str TSRMLS_DC);

void create_DecimalNoCheck(zval* obj, zval* zv TSRMLS_DC);

};

#endif
