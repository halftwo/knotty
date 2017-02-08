#ifndef vbs_Blob_h_
#define vbs_Blob_h_

#include "php_xic.h"


namespace vbs
{

extern zend_class_entry *classEntry_Blob;

bool init_Blob(TSRMLS_D);

bool create_Blob(zval* zv, zval* str TSRMLS_DC);


};

#endif
