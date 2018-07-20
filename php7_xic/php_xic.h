#ifndef PHP_XIC_H
#define PHP_XIC_H

#include "util.h"

#define PHP_XIC_EDITION     170615
#define PHP_XIC_REVISION    180720
#define PHP_XIC_RELEASE     15

#define PHP_XIC_VERSION     XS_TOSTR(PHP_XIC_EDITION) "." XS_TOSTR(PHP_XIC_REVISION) "." XS_TOSTR(PHP_XIC_RELEASE)

#define XIC_SO_VERSION		"2.0.0"


extern zend_module_entry xic_module_entry;
#define phpext_xic_ptr &xic_module_entry

#ifdef PHP_WIN32
#define PHP_XIC_API __declspec(dllexport)
#else
#define PHP_XIC_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(xic);
PHP_MSHUTDOWN_FUNCTION(xic);
PHP_MINFO_FUNCTION(xic);
PHP_RINIT_FUNCTION(xic);
PHP_RSHUTDOWN_FUNCTION(xic);


/*
   proto xic_Proxy xic_Engine::stringToProxy(string $str);
   void xic_Engine::setSecret(string $secret);
   string xic_Engine::getSecret();

   proto array xic_Proxy::invoke(string $method, array $args [, array $ctx]);
   proto void xic_Proxy::invoke_oneway(string $method, array $args [, array $ctx]);
   proto void xic_Proxy::setContext(array $ctx);
   proto array xic_Proxy::getContext();
   proto string xic_Proxy::service();

   proto bool xic_Exception::isRemote();
   proto string xic_Exception::getProxyString();
   proto string xic_Exception::getExname();
   proto string xic_Exception::getRaiser();
   proto array xic_Exception::getDetail();

   proto string v_Blob::toString();

   proto array v_Dict::toArray();

   proto string v_Decimal::toString();

   proto mixed v_Data()::getData();
   proto int v_Data()::getDescriptor();
*/


/* proto object xic_engine();
   Return a xic_Engine object.
   If error, return NULL.
 */
PHP_FUNCTION(xic_engine);


/* proto string xic_self_id();
 */
PHP_FUNCTION(xic_self_id);


/* proto string xic_self();
 */
PHP_FUNCTION(xic_self);


/* proto v_Blob vbs_blob(string $str);
 */
PHP_FUNCTION(vbs_blob);


/* proto v_Dict vbs_dict(array $arr);
 */
PHP_FUNCTION(vbs_dict);


/* proto v_Decimal vbs_decimal(string $str);
 */
PHP_FUNCTION(vbs_decimal);


/* proto v_Data vbs_data(mixed $data, int descriptor);
 */
PHP_FUNCTION(vbs_data);


/* proto string vbs_encode(mixed $arg);
   Return a encoded string.
 */
PHP_FUNCTION(vbs_encode);


/* proto string vbs_pack(array $arg);
   Return a encoded string.
 */
PHP_FUNCTION(vbs_pack);


/* proto int vbs_encode_write(resource $handle, mixed $arg);
   Return the number of bytes written, or FALSE on error.
 */
PHP_FUNCTION(vbs_encode_write);



/* proto mixed vbs_decode(string $arg [, int &$used])
   Return the decoded value 
   If error, return NULL (and $used is 0).
 */
PHP_FUNCTION(vbs_decode);


/* proto array vbs_unpack(string $arg, int $offset, int $num [, int &$used])
   Return the decoded values in an array.
   If error, return NULL (and $used is 0).
 */
PHP_FUNCTION(vbs_unpack);


PHP_FUNCTION(dlog);


ZEND_BEGIN_MODULE_GLOBALS(xic)
	zval the_engine;
	zval the_self;
	zval the_self_id;
ZEND_END_MODULE_GLOBALS(xic)



PHP_FUNCTION(xic_build_info);


zval *get_xic_self();

/* In every utility function you add that needs to use variables 
   in php_xic_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as XIC_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define XIC_G(v) TSRMG(xic_globals_id, zend_xic_globals *, v)
#else
#define XIC_G(v) (xic_globals.v)
#endif

#endif	/* PHP_XIC_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
