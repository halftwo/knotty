#ifndef XIC_ENGINE_H_
#define XIC_ENGINE_H_

#include "php_xic.h"
#include "xic_Proxy.h"
#include "Connection.h"
#include "xic/SecretBox.h"
#include <stdio.h>
#include <string>
#include <map>


namespace xic
{

class Engine;
typedef XPtr<Engine> EnginePtr;

extern zend_class_entry *classEntry_Engine;

bool init_Engine(TSRMLS_D);
bool create_Engine(zval* , const EnginePtr& en TSRMLS_DC);


class Engine: public XRefCount
{
	std::map<std::string, ProxyPtr> _proxyMap;
	std::map<std::string, ConnectionPtr> _conMap;
	SecretBoxPtr _secretBox;
public:
	virtual ~Engine();
	void finish();

	ProxyPtr stringToProxy(const std::string& proxy);

	ConnectionPtr makeConnection(const std::string& endpoint);

	SecretBoxPtr getSecretBox() const	{ return _secretBox; }

	std::string get_secret() const;
	void set_secret(const xstr_t& xs);
};


};

#endif
