#include "DemoServant.h"
#include "xslib/xlog.h"

#define DEMO_ST_VERSION	"170629.18"

static char build_info[] = "$build: demo_st-" DEMO_ST_VERSION " " __DATE__ " " __TIME__ " $";

static int run(int argc, char **argv, const xic::EnginePtr& engine)
{
	xlog_level = XLOG_DEBUG;
	engine->throb(build_info);
	xic::AdapterPtr adapter = engine->createAdapter();
	new DemoServant(engine->setting(), adapter);
	adapter->activate();
	engine->waitForShutdown();
	return 0;
}

int main(int argc, char **argv)
{
	SettingPtr setting = newSetting();
	setting->insert("xic.rlimit.nofile", "64");
	return xic::start_xic_st(run, argc, argv, setting);
}

