#ifndef XIC_PROXY_H_
#define XIC_PROXY_H_

#include "php_xic.h"
#include "Connection.h"
#include "xslib/rope.h"
#include <vector>


namespace xic
{

class Proxy;
typedef XPtr<Proxy> ProxyPtr;

class Engine;
typedef XPtr<Engine> EnginePtr;

extern zend_class_entry *classEntry_Proxy;

bool init_Proxy(TSRMLS_D);
bool create_Proxy(zval* , const ProxyPtr& prx TSRMLS_DC);


class Proxy: public XRefCount
{
	EnginePtr _engine;
	std::string _proxy;
	std::string _service;
	std::string _ctx;
	std::vector<ConnectionPtr> _cons;
	int _idx;

public:
	Proxy(const EnginePtr& engine, const std::string& proxy);
	virtual ~Proxy();

	const std::string& str() const		{ return _proxy; }
	const std::string& service() const 	{ return _service; }

	void set_context(const std::string& ctx) { _ctx = ctx; }
	const std::string& get_context() const	{ return _ctx; }
	ConnectionPtr getConnection() const	{ return _idx < _cons.size() ? _cons[_idx] : ConnectionPtr();  }

	xstr_t invoke(const xstr_t& method, const rope_t& rope, const std::string& ctx);
	void invoke_oneway(const xstr_t& method, const rope_t& rope, const std::string& ctx);
};


};

#endif
