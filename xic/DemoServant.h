#ifndef DemoServant_h_
#define DemoServant_h_

#include "ServantI.h"
#include "xslib/XLock.h"
#include <string>
#include <map>

#define DEMOSERVANT_CMDS	\
	CMD(time)		\
	CMD(echo)		\
	CMD(rubbish)		\
	CMD(discard)		\
	CMD(wait)		\
	CMD(rmi)		\
	CMD(throwException)	\
	CMD(selfProxy)		\
	CMD(setCallback)	\
	/* END OF CMDS */

class DemoServant: public xic::ServantI, private XMutex
{
	static xic::MethodTab::PairType _methodpairs[];
	static xic::MethodTab _methodtab;

	xic::EnginePtr _engine;
	xic::ProxyPtr _selfPrx;

	typedef std::map<std::string, xic::ProxyPtr> CallbackProxyMap;
	CallbackProxyMap _callbackMap;
public:
	DemoServant(const SettingPtr& setting, const xic::AdapterPtr& adapter);
	virtual ~DemoServant();

private:
	void callback_fiber();

#define CMD(X)  XIC_METHOD_DECLARE(X);
	DEMOSERVANT_CMDS
#undef CMD
};

#endif

