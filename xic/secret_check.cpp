#include "SecretBox.h"
#include "ShadowBox.h"
#include "Engine.h"
#include "xslib/Srp6a.h"
#include "xslib/opt.h"
#include "xslib/xbase32.h"
#include "xslib/xbase64.h"
#include "xslib/urandom.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <set>

#define SECRET_CHECK_VERSION	"17061418"

void display_version(const char *program)
{
	fprintf(stderr, 
"secret_check " SECRET_CHECK_VERSION "\n"
"$build: secret_check-" SECRET_CHECK_VERSION " " __DATE__ " " __TIME__ " $\n"
	);
	exit(1);
}

void usage(const char *program)
{
	fprintf(stderr, "Usage: %s <secret_file> <shadow_file>\n", program);
	fprintf(stderr, "Usage: %s <secret_file> <service>@tcp+<host>+<port>\n", program);
	fprintf(stderr,
"  -V                 display version information\n"
"\n"
		);
	exit(1);
}

static int run_xic_client(int argc, char **argv, const xic::EnginePtr& engine)
{
	std::string proxy = engine->setting()->getString("my.test.proxy");
	xstr_t xs = XSTR_CXX(proxy);
	xstr_t service;
	xstr_key_value(&xs, '@', &service, NULL);
	if (service.len == 0)
	{
		xstr_t x00 = XSTR_CONST("\x00");
		proxy = x00 + proxy;
	}

	xic::ProxyPtr prx = engine->stringToProxy(proxy);
	xstr_t method = XSTR_CONST("\x00ping");
	xic::QuestWriter qw(method);
	xic::ResultPtr result = prx->emitQuest(qw, xic::NULL_COMPLETION);
	xic::AnswerReader ar = result->takeAnswer(false);
	int ret = 0;
	if (ar.status())
	{
		xstr_t exname = ar.getXstr("exname");
		xstr_t msg = ar.getXstr("message");
		if (!ar.getBool("_local"))
			fputc('!', stderr);
		fprintf(stderr, "%.*s --- %.*s\n", XSTR_P(&exname), XSTR_P(&msg));

		if (!prx->getConnection() || prx->getConnection()->state() >= xic::Connection::ST_ERROR)
			ret = -1;
	}
	engine->shutdown();
	return ret;
}

int do_check(const char *secret_file, const char *shadow_file)
{
	SecretBoxPtr secret = SecretBox::createFromFile(secret_file);
	if (secret->count() == 0)
		throw XERROR_FMT(XError, "There is no items in the secret file `%s`", secret_file);

	if (strchr(shadow_file, '@') && strchr(shadow_file, '+'))
	{
		static char *args[] = { (char*)"secret_check", NULL };
		const char *proxy = shadow_file;
		SettingPtr setting = newSetting();
		setting->insert("xic.passport.secret", secret_file);
		setting->insert("my.test.proxy", proxy);
		int rc = xic::start_xic_pt(run_xic_client, 1, args, setting);
		printf("%s\n", rc < 0 ? "FAIL" : "PASS");
		return rc;
	}

	ShadowBoxPtr shadow = ShadowBox::createFromFile(shadow_file);
	size_t count = secret->count();
	std::set<std::string> checked;
	for (size_t i = 0; i < count; ++i)
	{
		xstr_t service, proto, host, identity, password;
		int port;
		secret->getItem(i, service, proto, host, port, identity, password);
		if (!checked.insert(std::string(identity + ":" + password)).second)
			continue;

		xstr_t method, paramId, hashId, salt, verifier;
		if (!shadow->getVerifier(identity, method, paramId, hashId, salt, verifier))
		{
			printf("MISS\t%.*s:%.*s\n", XSTR_P(&identity), XSTR_P(&password));
			continue;
		}

		int bits;
		uintmax_t g;
		xstr_t N;
		if (!shadow->getSrp6aParameter(paramId, bits, g, N))
		{
			printf("ERROR shadow file not consistent\n");
			continue;
		}

		Srp6aClientPtr srp6aclient = new Srp6aClient(g, N, bits, hashId);
		srp6aclient->set_identity(identity, password);
		srp6aclient->set_salt(salt);
		xstr_t v = srp6aclient->compute_v();
		if (!xstr_equal(&v, &verifier))
		{
			printf("FAIL\t%.*s:%.*s\n", XSTR_P(&identity), XSTR_P(&password));
			continue;
		}

		printf("PASS\t%.*s:%.*s\n", XSTR_P(&identity), XSTR_P(&password));
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *prog = argv[0];
	bool show_version = false;
	int optend;

	OPT_BEGIN(argc, argv, &optend)
	{
	case 'V':
		show_version = true;
		break;
	default:
		usage(prog);
	} OPT_END();

	if (show_version)
		display_version(prog);

	if (argc - optend < 2)
		usage(prog);

	try {
		int rc = do_check(argv[optend], argv[optend + 1]);
		if (rc < 0)
			exit(1);
	}
	catch (std::exception& ex)
	{
		fprintf(stderr, "ERROR %s\n", ex.what());
		exit(1);
	}
	return 0;
}

