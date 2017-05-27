#include "DemoClientImp.h"

int demo_client_doit(const xic::EnginePtr& engine, const std::string& serverAddr)
{
	SecretBoxPtr sb = SecretBox::createFromContent("@++=hello:world");
	engine->setSecretBox(sb);

	std::string proxy = "Demo @tcp+" + serverAddr;
	if (serverAddr.find('+') == std::string::npos)
		proxy += "+5555";

	xic::ProxyPtr prx = engine->stringToProxy(proxy);

	{
		xic::QuestWriter qw("echo");
		qw.param("a", "hello, world!");
		qw.param("b", random());
		qw.param("c", (double)random() / RAND_MAX);
		xic::AnswerReader ar = prx->request(qw);
		xstr_t a = ar.wantXstr("a");
		int b = ar.wantInt("b");
		double c = ar.wantFloating("c");
		printf("----------------------------- echo\n");
		printf("a = %.*s\n", XSTR_P(&a));
		printf("b = %d\n", b);
		printf("c = %g\n", c);
	}

	{
		xic::QuestWriter qw("time");
		xic::AnswerReader ar = prx->request(qw);
		xstr_t con = ar.getXstr("con");
		xic::VDict vd = ar.wantVDict("strftime");
		xstr_t utc = vd.wantXstr("utc");
		xstr_t local = vd.wantXstr("local");
		printf("----------------------------- time\n");
		printf("  con=%.*s\n", XSTR_P(&con));
		printf(" time=%jd\n", ar.wantInt("time"));
		printf("  utc=%.*s\n", XSTR_P(&utc));
		printf("local=%.*s\n", XSTR_P(&local));
	}

	return 0;
}

