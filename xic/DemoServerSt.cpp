#include "DemoServant.h"
#include "xslib/xlog.h"

static char compile_time[] = "$compile: " __DATE__ " " __TIME__ " $";

static int run(int argc, char **argv, const xic::EnginePtr& engine)
{
	xlog_level = XLOG_DEBUG;
	engine->throb(compile_time);
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

