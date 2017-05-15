#include "DemoClientImp.h"

int demo_client_doit(const xic::EnginePtr& engine)
{
	SecretBoxPtr sb = SecretBox::createFromContent("@++=complex:complicated");
	engine->setSecretBox(sb);
	std::string proxy = "Tester @tcp++5555";
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
		xstr_t utc = ar.wantXstr("utc");
		xstr_t local = ar.wantXstr("local");
		printf("----------------------------- time\n");
		printf(" time=%jd\n", ar.wantInt("time"));
		printf("  utc=%.*s\n", XSTR_P(&utc));
		printf("local=%.*s\n", XSTR_P(&local));
	}

	return 0;
}

