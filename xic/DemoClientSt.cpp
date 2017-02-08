#include "Engine.h"

static int run(int argc, char **argv, const xic::EnginePtr& engine)
{
	SecretBoxPtr sb = SecretBox::createFromContent("@++=complex:complicated");
	engine->setSecretBox(sb);
	std::string proxy = "Tester @tcp++5555";
	xic::ProxyPtr prx = engine->stringToProxy(proxy);
	xic::QuestWriter qw("echo");
	qw.param("a", "hello, world!");
	qw.param("b", random());
	qw.param("c", (double)random() / RAND_MAX);
	xic::AnswerReader ar = prx->request(qw);
	xstr_t a = ar.wantXstr("a");
	int b = ar.wantInt("b");
	double c = ar.wantFloating("c");
	printf("a = %.*s\n", XSTR_P(&a));
	printf("b = %d\n", b);
	printf("c = %g\n", c);
	engine->shutdown();
	return 0;
}

int main(int argc, char **argv)
{
	SettingPtr setting = newSetting();
	setting->insert("xic.rlimit.nofile", "64");
	return xic::start_xic_st(run, argc, argv, setting);
}

