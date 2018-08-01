#include "ServantI.h"
#include "dlog/dlog.h"
#include "xslib/XThread.h"
#include <unistd.h>

#define DEMOCALLBACK_VERSION	"170629.18"

static char build_info[] = "$build: democallback-" DEMOCALLBACK_VERSION " " __DATE__ " " __TIME__ " $";

#define CALLBACKSERVANT_CMDS	\
	CMD(cb_time)		\
	/* END OF CMDS */

class CallbackServant: public xic::ServantI
{
	static xic::MethodTab::PairType _methodpairs[];
	static xic::MethodTab _methodtab;

	xic::AdapterPtr _adapter;
	xic::ProxyPtr _demoPrx;
	xic::ProxyPtr _selfPrx;
public:
	CallbackServant(const xic::AdapterPtr& adapter, const xic::ProxyPtr& demoPrx);
	virtual ~CallbackServant();

private:
	void ping_fiber();

#define CMD(X)  XIC_METHOD_DECLARE(X);
	CALLBACKSERVANT_CMDS
#undef CMD
};

xic::MethodTab::PairType CallbackServant::_methodpairs[] = {
#define CMD(X)  { XS_TOSTR(X), XIC_METHOD_CAST(CallbackServant, X) },
	CALLBACKSERVANT_CMDS
#undef CMD
};
xic::MethodTab CallbackServant::_methodtab(_methodpairs, XS_ARRCOUNT(_methodpairs));

CallbackServant::CallbackServant(const xic::AdapterPtr& adapter, const xic::ProxyPtr& demoPrx)
	: ServantI(&_methodtab), _adapter(adapter), _demoPrx(demoPrx)
{
	std::string service = "DemoCallback." + adapter->getEngine()->uuid();

	xref_inc();
	_selfPrx = adapter->addServant(service, this);
	XThread::create(this, &CallbackServant::ping_fiber);
	xref_dec_only();
}

CallbackServant::~CallbackServant()
{
}

void CallbackServant::ping_fiber()
{
	xic::ConnectionPtr lastCon;
	while(true)
	{
		try {
			xic::QuestWriter ping(xic::x00ping);
			_demoPrx->request(ping);
			xic::ConnectionPtr con = _demoPrx->getConnection();

			while (lastCon != con)
			{
				lastCon = con;
				con->setAdapter(_adapter);
				xic::QuestWriter qw("setCallback");
				qw("callback", _selfPrx->service());
				_demoPrx->request(qw);
				con = _demoPrx->getConnection();
			}
		}
		catch (std::exception& ex)
		{
			dlog("EXCEPTION", "%s", ex.what());
		}
		sleep(55);
	}
}

XIC_METHOD(CallbackServant, cb_time)
{
	xic::VDict args = quest->args();
	time_t t = args.getInt("time", LONG_MIN);
	if (t == LONG_MIN)
	{
		time(&t);
	}

        xic::AnswerWriter aw;
	aw.param("con", current.con->info());
        aw.param("time", t);

	xic::VDictWriter dw = aw.paramVDict("strftime");

        char buf[32];
	dlog_utc_time_str(t, buf);
        dw.kv("utc", buf);

	dlog_local_time_str(t, buf);
        dw.kv("local", buf);

        return aw;
}

static int run(int argc, char **argv, const xic::EnginePtr& engine)
{
	std::string serverAddr = (argc > 1) ? argv[1] : "";

	SecretBoxPtr sb = SecretBox::createFromContent("@++=hello:world");
	engine->setSecretBox(sb);

	std::string proxy = "Demo @tcp+" + serverAddr;
	if (serverAddr.find('+') == std::string::npos)
		proxy += "+5555";
	xic::ProxyPtr prx = engine->stringToProxy(proxy);

	xic::AdapterPtr adapter = engine->createSlackAdapter();
	new CallbackServant(adapter, prx);

	engine->throb(build_info);
	adapter->activate();
	engine->waitForShutdown();
	return 0;
}

int main(int argc, char **argv)
{
	SettingPtr setting = newSetting();
	setting->insert("xic.sample.server", "1");
	return xic::start_xic_pt(run, argc, argv, setting);
}

