#ifndef DemoServant_h_
#define DemoServant_h_

#include "ServantI.h"

#define CMD_LIST		\
	CMD(time)		\
	CMD(echo)		\
	CMD(rubbish)		\
	CMD(discard)		\
	CMD(wait)		\
	CMD(rmi)		\
	CMD(throwException)	\
	CMD(selfProxy)		\
	/* end list */

class DemoServant: public xic::ServantI
{
	static xic::MethodTab::PairType _methodpairs[];
	static xic::MethodTab _methodtab;

	xic::EnginePtr _engine;
	xic::ProxyPtr _selfPrx;
public:
	DemoServant(const SettingPtr& setting, const xic::AdapterPtr& adapter);
	virtual ~DemoServant();

#define CMD(X)  XIC_METHOD_DECLARE(X);
	CMD_LIST
#undef CMD
};

#endif

