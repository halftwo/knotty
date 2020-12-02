#include "xic_Engine.h"
#include "xic_Proxy.h"
#include "xslib/xsdef.h"

using namespace xic;

namespace xic
{
zend_class_entry *classEntry_Engine = 0;
};

static zend_object_handlers _handlers;


static void delete_Engine(zend_object *p TSRMLS_DC)
{
	MyObject<Engine>* obj = static_cast<MyObject<Engine>*>((void *)p);
	obj->ptr.reset();
}

static zend_object* new_Engine(zend_class_entry *ce TSRMLS_DC)
{
	MyObject<Engine>* obj = MyObject<Engine>::create(ce TSRMLS_CC);
	assert(obj);

	zend_object *o = static_cast<zend_object*>((void *)obj);
	o->handlers = &_handlers;

	_handlers.free_obj = delete_Engine;
	return o;
}

PHP_METHOD(xic_Engine, __construct)
{
	raise_Exception(0 TSRMLS_CC, "xic_Engine cannot be instantiated, use xic_engine() function instead.");
	return;
}

PHP_METHOD(xic_Engine, __toString)
{
	RETURN_STRING("xic_Engine()");
}

/* proto xic_Proxy xic_Engine::stringToProxy(string)
 */
PHP_METHOD(xic_Engine, stringToProxy)
{
	char *str;
	size_t slen;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &slen) != SUCCESS)
	{
		RETURN_NULL();
	}

	std::string s(str, slen);
	EnginePtr engine = MyObject<Engine>::get(getThis() TSRMLS_CC);
	ProxyPtr proxy = engine->stringToProxy(s);

	std::string ctx = get_default_ctx();
	if (!ctx.empty())
	{
		proxy->set_context(ctx);
	}

	create_Proxy(return_value, proxy TSRMLS_CC);
}

/* proto void xic_Engine::setSecret(string $secret)
 */
PHP_METHOD(xic_Engine, setSecret)
{
	char *str;
	size_t slen;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &str, &slen) != SUCCESS)
	{
		raise_Exception(0 TSRMLS_CC, "Invalid parameter");
		RETURN_NULL();
	}

	EnginePtr engine = MyObject<Engine>::get(getThis() TSRMLS_CC);

	try {
		xstr_t xs = XSTR_INIT((uint8_t*)str, (ssize_t)slen);
		engine->set_secret(xs);
	}
	catch (std::exception& ex)
	{
		raise_Exception(0 TSRMLS_CC, "%s", ex.what());
	}
}

/* proto string xic_Engine::getSecret()
 */
PHP_METHOD(xic_Engine, getSecret)
{
	EnginePtr engine = MyObject<Engine>::get(getThis() TSRMLS_CC);
	std::string s = engine->get_secret();

	RETURN_STRINGL(s.data(), s.length());
}


ZEND_BEGIN_ARG_INFO_EX(arginfo_construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_toString, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_stringToProxy, 0, 0, 1)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_setSecret, 0, 0, 1)
	ZEND_ARG_INFO(0, secret)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_getSecret, 0, 0, 0)
ZEND_END_ARG_INFO()

static zend_function_entry _methods[] = {
	PHP_ME(xic_Engine, __construct, arginfo_construct, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Engine, __toString, arginfo_toString, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Engine, stringToProxy, arginfo_stringToProxy, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Engine, setSecret, arginfo_setSecret, ZEND_ACC_PUBLIC)
	PHP_ME(xic_Engine, getSecret, arginfo_getSecret, ZEND_ACC_PUBLIC)
	{ NULL, NULL, NULL }
};


namespace xic
{

bool init_Engine(TSRMLS_D)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "xic_Engine", _methods);
	classEntry_Engine = zend_register_internal_class(&ce TSRMLS_CC);
	classEntry_Engine->create_object = new_Engine;
	classEntry_Engine->ce_flags |= ZEND_ACC_FINAL;

	_handlers = *zend_get_std_object_handlers();

	return true;
}

bool create_Engine(zval* zv, const EnginePtr& en TSRMLS_DC)
{
	object_init_ex(zv, classEntry_Engine);

	MyObject<Engine> *obj = MyObject<Engine>::extract(zv TSRMLS_CC);
	obj->ptr = en;
	return true;
}

Engine::~Engine()
{
}

void Engine::finish()
{
	_proxyMap.clear();

	for (std::map<std::string, ConnectionPtr>::iterator iter = _conMap.begin(); iter != _conMap.end(); ++iter)
	{
		iter->second->close();
	}

	_conMap.clear();
}

static std::string normalize(const std::string& str)
{
	xstr_t xs = XSTR_CXX(str);
	char *buf = (char *)alloca(xs.len + xstr_count_char(&xs, '@') + 1);
	char *p = buf;

	xstr_t service;
	xstr_delimit_char(&xs, '@', &service);
	xstr_trim(&service);
	if (service.len == 0)
		goto error;

	pptr_xio.write(&p, service.data, service.len);

	xstr_t endpoint;
	while (xstr_delimit_char(&xs, '@', &endpoint))
	{
		xstr_trim(&endpoint);
		if (endpoint.len)
		{
			xstr_t token;
			xstr_token_space(&endpoint, &token);
			if (xstr_count_char(&token, '+') != 2)
				goto error;

			pptr_xio.write(&p, " @", 2);
			pptr_xio.write(&p, token.data, token.len);
			while (xstr_token_space(&endpoint, &token))
			{
				if (xstr_count_char(&token, '=') != 1)
					goto error;
				pptr_xio.write(&p, " ", 1);
				pptr_xio.write(&p, token.data, token.len);
			}
		}
	}
	return std::string(buf, p - buf);
error:
	return std::string();
}

ProxyPtr Engine::stringToProxy(const std::string& proxy)
{
	std::string str = normalize(proxy);
	std::map<std::string, ProxyPtr>::iterator iter = _proxyMap.find(str);
	if (iter != _proxyMap.end())
		return iter->second;

	ProxyPtr prx(new Proxy(EnginePtr(this), str));
	_proxyMap.insert(_proxyMap.end(), std::make_pair(str, prx));
	return prx;
}

ConnectionPtr Engine::makeConnection(const std::string& str)
{
	std::map<std::string, ConnectionPtr>::iterator iter = _conMap.find(str);
	if (iter != _conMap.end())
		return iter->second;

	ConnectionPtr con(new Connection(this, str));
	_conMap.insert(_conMap.end(), std::make_pair(str, con));
	return con;
}

void Engine::set_secret(const xstr_t& xs)
{
	SecretBoxPtr sb = SecretBox::createFromContent(make_string(xs));
	_secretBox.swap(sb);
}

std::string Engine::get_secret() const
{
	std::string s;
	if (_secretBox)
		s = _secretBox->getContent();
	return s;
}


}

