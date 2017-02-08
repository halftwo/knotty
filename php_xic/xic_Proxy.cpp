#include "xic_Proxy.h"
#include "xic_Engine.h"
#include "xic_Exception.h"
#include "vbs_Blob.h"
#include "vbs_Dict.h"
#include "vbs_codec.h"
#include "xslib/xsdef.h"
#include "xslib/vbs_pack.h"
#include "xslib/ScopeGuard.h"
#include "zend.h"
#include <sstream>
#include <algorithm>

using namespace std;
using namespace xic;

namespace xic
{
zend_class_entry *classEntry_Proxy = 0;
};

static zend_object_handlers _handlers;

static void delete_Proxy(void *p TSRMLS_DC)
{
	MyObject<Proxy>* obj = static_cast<MyObject<Proxy>*>(p);
	obj->ptr.reset();
	zend_objects_free_object_storage(static_cast<zend_object*>(p) TSRMLS_CC);
}

static zend_object_value new_Proxy(zend_class_entry *ce TSRMLS_DC)
{
	MyObject<Proxy>* obj = MyObject<Proxy>::create(ce TSRMLS_CC);
	assert(obj);

	zend_object_value retval;
	retval.handle = zend_objects_store_put(obj, NULL, delete_Proxy, NULL TSRMLS_CC);
	retval.handlers = &_handlers;

	return retval;
}

PHP_METHOD(xic_Proxy, __construct)
{
	raise_Exception(0 TSRMLS_CC, "xic_Proxy cannot be instantiated, use xic_engine()->stringToProxy() instead.");
	return;
}

PHP_METHOD(xic_Proxy, __toString)
{
	ProxyPtr prx = MyObject<Proxy>::get(getThis() TSRMLS_CC);
	const std::string& s = prx->str();
	char buf[1024];
	int len = snprintf(buf, sizeof(buf), "xic_Proxy(%.*s)", (int)(sizeof(buf)-24), s.c_str());
	RETURN_STRINGL(buf, len, 1);
}

/* proto string xic_Proxy::service()
 */
PHP_METHOD(xic_Proxy, service)
{
	ProxyPtr prx = MyObject<Proxy>::get(getThis() TSRMLS_CC);
	const xstr_t& service = prx->service();
	RETURN_STRINGL((char *)service.data, service.len, 1);
}


/* proto void xic_Proxy::setContext(array $ctx)
 */
PHP_METHOD(xic_Proxy, setContext)
{
	ProxyPtr prx = MyObject<Proxy>::get(getThis() TSRMLS_CC);
	zval *ctx = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &ctx) == FAILURE
		|| (Z_TYPE_P(ctx) != IS_ARRAY && !VALID_OBJECT(ctx, vbs::classEntry_Dict)))
	{
		raise_Exception(0 TSRMLS_CC, "Invalid parameter");
		return;
	}

	update_ctx_caller(ctx);
	vbs_packer_t pk;
	std::ostringstream os;
	vbs_packer_init(&pk, ostream_xio.write, (std::ostream*)&os, 1);
	vbs::v_encode_ctx(&pk, ctx TSRMLS_CC);
	if (pk.error)
	{
		raise_Exception(0 TSRMLS_CC, "Invalid context");
		return;
	}
	prx->set_context(os.str());
}

/* proto array xic_Proxy::getContext()
 */
PHP_METHOD(xic_Proxy, getContext)
{
	ProxyPtr prx = MyObject<Proxy>::get(getThis() TSRMLS_CC);
	const std::string& str = prx->get_context();

	if (str.empty())
	{
		array_init(return_value);
	}
	else
	{
		zval *ctx = NULL;
		vbs_unpacker_t uk;
		vbs_unpacker_init(&uk, (unsigned char *)str.data(), str.length(), 1);
		vbs::v_decode_r(&uk, &ctx TSRMLS_CC);
		RETURN_ZVAL(ctx, 1, 1);
	}
}

/* proto array xic_Proxy::invoke(string $method, array $args [, array $ctx])
 */
PHP_METHOD(xic_Proxy, invoke)
{
	ProxyPtr prx = MyObject<Proxy>::get(getThis() TSRMLS_CC);
	char *name;
	int nlen;
	zval *args = NULL;
	zval *ctx = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|z!", &name, &nlen, &args, &ctx) == FAILURE)
	{
		raise_Exception(0 TSRMLS_CC, "Invalid parameter");
		return;
	}

	xstr_t method = XSTR_INIT((unsigned char *)name, nlen);
	try {
		vbs_packer_t pk;
		rope_t args_rope = ROPE_INIT(4096, NULL, NULL);
		ON_BLOCK_EXIT(rope_finish, &args_rope);
		vbs_packer_init(&pk, rope_xio.write, &args_rope, 100);
		vbs::v_encode_args(&pk, args TSRMLS_CC);

		std::string ctx_string;
		if (ctx)
		{
			update_ctx_caller(ctx);
			std::ostringstream os;
			vbs_packer_init(&pk, ostream_xio.write, (std::ostream*)&os, 1);
			vbs::v_encode_ctx(&pk, ctx TSRMLS_CC);
			if (pk.error)
				throw XERROR_MSG(XError, "Invalid context");
			ctx_string = os.str();
		}

		xstr_t res = prx->invoke(method, args_rope, ctx_string);
		ON_BLOCK_EXIT(free, res.data);
		vbs_unpacker_t uk = VBS_UNPACKER_INIT(res.data, res.len, 100);
		int64_t txid;
		if (vbs_unpack_int64(&uk, &txid) < 0 || txid == 0)
		{
			throw XERROR_MSG(XError, "decode answer txid failed");
		}

		intmax_t status;
		if (vbs_unpack_integer(&uk, &status) < 0)
		{
			throw XERROR_MSG(XError, "decode answer status failed");
		}

		zval *z = NULL;
		if (!vbs::v_decode_r(&uk, &z TSRMLS_CC))
		{
			throw XERROR_MSG(XError, "decode answer args failed");
		}

		if (Z_TYPE_P(z) != IS_ARRAY)
		{
			zval_ptr_dtor(&z);
			throw XERROR_MSG(XError, "answer args not a dict");
		}

		if (status)
		{
			ConnectionPtr con = prx->getConnection();
			const std::string proxy = prx->service() + " @" + con->endpoint();
			xic::raise_RemoteException(proxy, z TSRMLS_CC);
			zval_ptr_dtor(&z);
			return;
		}

		ZVAL_ZVAL(return_value, z, 1, 1);
	}
	catch (std::exception& ex)
	{
		xic::raise_LocalException(prx->str(), ex TSRMLS_CC);
		return;
	}
}

/* proto void xic_Proxy::invoke_oneway(string $method, array $args [, array $ctx])
 */
PHP_METHOD(xic_Proxy, invoke_oneway)
{
	ProxyPtr prx = MyObject<Proxy>::get(getThis() TSRMLS_CC);
	char *name;
	int nlen;
	zval *args = NULL;
	zval *ctx = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|z!", &name, &nlen, &args, &ctx) == FAILURE)
	{
		raise_Exception(0 TSRMLS_CC, "Invalid parameter");
		return;
	}

	xstr_t method = XSTR_INIT((unsigned char *)name, nlen);
	try {
		vbs_packer_t pk;
		rope_t args_rope = ROPE_INIT(4096, NULL, NULL);
		ON_BLOCK_EXIT(rope_finish, &args_rope);
		vbs_packer_init(&pk, rope_xio.write, &args_rope, 100);
		vbs::v_encode_args(&pk, args TSRMLS_CC);

		std::string ctx_string;
		if (ctx)
		{
			update_ctx_caller(ctx);
			std::ostringstream os;
			vbs_packer_init(&pk, ostream_xio.write, (std::ostream*)&os, 1);
			vbs::v_encode_ctx(&pk, ctx TSRMLS_CC);
			if (pk.error)
				throw XERROR_MSG(XError, "Invalid context");
			ctx_string = os.str();
		}

		prx->invoke_oneway(method, args_rope, ctx_string);
	}
	catch (std::exception& ex)
	{
		xic::raise_LocalException(prx->str(), ex TSRMLS_CC);
		return;
	}
}

static zend_function_entry _methods[] = {
	PHP_ME(xic_Proxy, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Proxy, __toString, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Proxy, service, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Proxy, setContext, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Proxy, getContext, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Proxy, invoke, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Proxy, invoke_oneway, NULL, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};

namespace xic
{

bool init_Proxy(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "xic_Proxy", _methods);
	classEntry_Proxy = zend_register_internal_class(&ce TSRMLS_CC);
	classEntry_Proxy->create_object = new_Proxy;
	classEntry_Proxy->ce_flags |= ZEND_ACC_FINAL_CLASS;

	_handlers = *zend_get_std_object_handlers();
//	_handlers.clone_obj = handleClone;
//	_handlers.get_method = handle_GetMethod;
//	_handlers.compare_objects = handleCompare;

	return true;
}

bool create_Proxy(zval *zv, const ProxyPtr& prx TSRMLS_DC)
{
	object_init_ex(zv, classEntry_Proxy);

	MyObject<Proxy> *obj = MyObject<Proxy>::extract(zv TSRMLS_CC);
	obj->ptr = prx;
	return true;
}

struct ConLess
{
	bool operator()(const ConnectionPtr& a, const ConnectionPtr& b)
	{
		return a->priority() > b->priority();
	}
};

Proxy::Proxy(const EnginePtr& engine, const std::string& x)
	: _engine(engine), _proxy(x), _idx(0)
{
	xstr_t xs = XSTR_CXX(_proxy);

	xstr_delimit_char(&xs, '@', &_service);
	xstr_trim(&_service);

	xstr_t endpoint;
	while (xstr_delimit_char(&xs, '@', &endpoint))
	{
		xstr_trim(&endpoint);
		if (endpoint.len)
		{
			std::string s((char *)endpoint.data, endpoint.len);
			ConnectionPtr con = _engine->makeConnection(s);
			_cons.push_back(con);
		}
	}

	if (_cons.size())
	{
		stable_sort(&_cons[0], &_cons[_cons.size()], ConLess());
	}
}

Proxy::~Proxy()
{
}

xstr_t Proxy::invoke(const xstr_t& method, const rope_t& args, const std::string& ctx)
{
	int size = _cons.size();
	if (size > 0)
	{
		const std::string& ctxstr = !ctx.empty() ? ctx : _ctx;
		try {
			return _cons[_idx]->invoke(_service, method, args, ctxstr);
		}
		catch (std::exception& ex)
		{
			int idx = (_idx + 1) % size;
			if (idx == _idx)
				throw;

			_idx = idx;
			return _cons[_idx]->invoke(_service, method, args, ctxstr);
		}
	}

	throw XERROR_MSG(XError, "No endpoint");
}

void Proxy::invoke_oneway(const xstr_t& method, const rope_t& args, const std::string& ctx)
{
	int size = _cons.size();
	if (size > 0)
	{
		const std::string& ctxstr = !ctx.empty() ? ctx : _ctx;
		try {
			return _cons[_idx]->invoke_oneway(_service, method, args, ctxstr);
		}
		catch (std::exception& ex)
		{
			int idx = (_idx + 1) % size;
			if (idx == _idx)
				throw;

			_idx = idx;
			return _cons[_idx]->invoke_oneway(_service, method, args, ctxstr);
		}
	}

	throw XERROR_MSG(XError, "No endpoint");
}

}

