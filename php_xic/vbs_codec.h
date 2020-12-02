#ifndef vbs_codec_h_
#define vbs_codec_h_

#include "php.h"
#include "php_ini.h"
#include "zend_interfaces.h"
#include "zend_exceptions.h"
#include "xslib/xsdef.h"
#include "xslib/vbs_pack.h"
#include "xslib/XError.h"
#include "util.h"

namespace vbs
{


XE_(XError, EncodeError);

void v_encode_r(vbs_packer_t *job, zval *val TSRMLS_DC);

bool v_decode_r(vbs_unpacker_t *job, zval *zz TSRMLS_DC);


void v_pack(vbs_packer_t *job, zval *arr TSRMLS_DC);

bool v_unpack(vbs_unpacker_t *job, long num, zval *zz TSRMLS_DC);


void v_encode_args_without_headtail(vbs_packer_t *job, zval *args TSRMLS_DC);

void v_encode_args(vbs_packer_t *job, zval *args TSRMLS_DC);

#define v_encode_ctx v_encode_args


};

#endif
