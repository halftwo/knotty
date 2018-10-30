#include "xic/Engine.h"
#include "xic/XicMessage.h"
#include "xic/XicException.h"
#include "xic/VData.h"
#include "dlog/dlog.h"
#include "xslib/Setting.h"
#include "xslib/StringHashTab.h"
#include "xslib/ScopeGuard.h"
#include "xslib/xlog.h"
#include "xslib/XThread.h"
#include "xslib/rope.h"
#include "xslib/path.h"
#include "xslib/xio.h"
#include "xslib/urandom.h"
#include "FcgiClient.h"
#include <errno.h>
#include <vector>

#define X4FCGI_V_EDITION          170802
#define X4FCGI_V_REVISION         181030
#define X4FCGI_V_RELEASE          20

#define X4FCGI_VERSION            XS_TOSTR(X4FCGI_V_EDITION) "." XS_TOSTR(X4FCGI_V_REVISION) "." XS_TOSTR(X4FCGI_V_RELEASE)

static char build_info[] = "$build: x4fcgi-" X4FCGI_VERSION " " __DATE__ " " __TIME__ " $";

static pthread_once_t dispatcher_once = PTHREAD_ONCE_INIT;
static XEvent::DispatcherPtr the_dispatcher;
static int fcgi_thread_number = 2;

static void start_dispatcher()
{
	fcgi_thread_number = XS_CLAMP(fcgi_thread_number, 1, 32);
        the_dispatcher = XEvent::Dispatcher::create();
        the_dispatcher->setThreadPool(fcgi_thread_number, fcgi_thread_number, 1024*256);
        the_dispatcher->start();
}

class X4fcgiServant: public xic::Servant
{
	FcgiClientPtr _fcgi;
	bool _displayError;

public:
	X4fcgiServant(const SettingPtr& setting);
	virtual ~X4fcgiServant();
	xic::AnswerPtr process(const xic::QuestPtr& quest, const xic::Current& current);
};


X4fcgiServant::X4fcgiServant(const SettingPtr& setting)
{
	fcgi_thread_number = setting->getInt("Fcgi.ThreadPoolNumber", 2);

	_displayError = setting->getBool("Fcgi.DisplayError");

	FcgiConfig conf;
	conf.location = setting->wantString("Fcgi.Location");
	conf.conmax = setting->getInt("Fcgi.ConnectionMax", 2);
	conf.timeout = setting->getInt("Fcgi.Timeout", -1);
	// NB: Because the FastCGI protocol has no way to close the connection
 	// gracefully. We should set Fcgi.RequestMax to 1 exactly.
	conf.reqmax = setting->getInt("Fcgi.RequestMax", 1);
	conf.autoretry = setting->getBool("Fcgi.AutoRetry", false);
	conf.entryfile = setting->getString("Fcgi.EntryFile", "run.php");

	std::string rootdir = setting->wantPathname("Fcgi.RootDir");
	char *buf = XS_ALLOC(char, rootdir.length() + 1);
	ON_BLOCK_EXIT(free, buf);
	size_t n = path_normalize_mem(buf, rootdir.data(), rootdir.length());
	if (n == 0 || (n == 1 && buf[0] == '/'))
		throw XERROR_FMT(XError, "invalid Fcgi.RootDir %s", rootdir.c_str());

	if (buf[n - 1] == '/')
		buf[--n] = 0;

	conf.rootdir = std::string(buf, n);

        pthread_once(&dispatcher_once, start_dispatcher);
	_fcgi.reset(new FcgiClient(the_dispatcher, conf));
}

X4fcgiServant::~X4fcgiServant()
{
}


class FCallback: public FcgiCallback
{
	xic::WaiterPtr _waiter;
	bool _displayError;
public:
	FCallback(const xic::WaiterPtr& waiter, bool displayError)
		: _waiter(waiter), _displayError(displayError)
	{
	}

	virtual void response(const FcgiAnswerPtr& fa);

	virtual void response(const std::exception& ex)
	{
		_waiter->response(ex);
	}
};

void FCallback::response(const FcgiAnswerPtr& fa)
{
	try {
		assert(fa);
		ssize_t answer_size = fa->xic_answer_size();
		if (answer_size <= 0)
		{
			rope_block_t *block;

			xstr_t header = xstr_null;
			block = NULL;
			rope_next_block(fa->get_header(), &block, &header.data, &header.len);
			const char *more_header = (fa->get_header()->block_count > 1) ? " ... \n\n" : ""; 

			xstr_t body = xstr_null;
			block = NULL;
			rope_next_block(fa->get_content(), &block, &body.data, &body.len);
			const char *more_body = (fa->get_content()->block_count > 1) ? " ... " : ""; 

			xstr_t err_xs = xstr_null;
			block = NULL;
			rope_next_block(fa->get_stderr(), &block, &err_xs.data, &err_xs.len);
			const char *more_err = (fa->get_stderr()->block_count > 1) ? " ... " : ""; 
			const char *stderr_prefix = err_xs.len > 0 ? "\n__STDERR__:\n" : "";

			xdlog(NULL, NULL, "STDOUT", fa->request_uri(), "%.*s%s", XSTR_P(&body), more_body);
			xdlog(NULL, NULL, "WARNING", fa->request_uri(), "Invalid x4fcgi answer from the fcgi server [%s]", fa->script_filename());

			if (fa->status() != 200)
			{
				if (!_displayError)
				{
					throw XERROR_CODE_FMT(XError, fa->status(),
						"Status(%d) returned from fcgi server", fa->status());
				}

				throw XERROR_CODE_FMT(XError, fa->status(),
					"Status(%d) returned from fcgi server [%s]:\n%.*s%s%.*s%s%s%.*s%s", fa->status(),
					fa->script_filename(),
					XSTR_P(&header), more_header, XSTR_P(&body), more_body,
					stderr_prefix, XSTR_P(&err_xs), more_err);
			}

			if (!_displayError)
			{
				throw XERROR_CODE_FMT(XError, 500, "Invalid x4fcgi answer from fcgi server (maybe php-fpm). You can see the output details from the fcgi server in dlog. Or set Fcgi.DisplayError=true in conf.x4fcgi file to view the output details directly here in the answer. Don't forget restart x4fcgi server after modifying conf.x4fcgi file.");
			}

			throw XERROR_CODE_FMT(XError, 500, 
				"Invalid x4fcgi answer from fcgi server [%s]:\n%.*s%s%.*s%s%s%.*s%s",
				fa->script_filename(),
				XSTR_P(&header), more_header, XSTR_P(&body), more_body,
				stderr_prefix, XSTR_P(&err_xs), more_err);
		}

		const rope_t *err = fa->get_stderr();
		xic::AnswerPtr answer;
		if (_displayError && err->length > 0)
		{
			xstr_t stderr_xs = XSTR_CONST("__x4fcgi_stderr__");
			int len = vbs_size_of_string(stderr_xs.len) + vbs_size_of_string(err->length);
			answer = xic::Answer::create(answer_size + len);
			xstr_t body = answer->body();
			fa->xic_answer_copy(body.data);
			char *p = (char *)body.data + answer_size - 1;
			vbs_packer_t pk = VBS_PACKER_INIT(pptr_xio.write, &p, -1);
			pk.depth = 1;	// We already in the dict.
			vbs_pack_xstr(&pk, &stderr_xs);
			vbs_pack_head_of_string(&pk, err->length);
			vbs_pack_raw_rope(&pk, err);
			vbs_pack_tail(&pk);
		}
		else
		{
			answer = xic::Answer::create(answer_size);
			xstr_t body = answer->body();
			fa->xic_answer_copy(body.data);
		}

		/* unpack to check it's a valid vbs encoded answer. */
		answer->unpack_body();
		answer->args_dict();
		_waiter->response(answer);
	}
	catch (std::exception& ex)
	{
		_waiter->response(ex);
	}
	catch (...)
	{
		XERROR_VAR_MSG(XError, ex, "Unknown Exception");
		_waiter->response(ex);
	}
}

xic::AnswerPtr X4fcgiServant::process(const xic::QuestPtr& quest, const xic::Current& current)
{
	bool oneway = quest->txid();
	FcgiCallbackPtr callback(oneway ? new FCallback(current.asynchronous(), _displayError) : 0);
	FcgiQuestPtr fq(FcgiQuest::create(_fcgi, callback, quest, current.con->endpoint()));
	_fcgi->process(fq);
	return oneway ? xic::ONEWAY_ANSWER : xic::ASYNC_ANSWER;
}


static int run(int argc, char **argv, const xic::EnginePtr& engine)
{
	engine->throb(build_info);
	xic::AdapterPtr adapter = engine->createAdapter();
	adapter->setDefaultServant(new X4fcgiServant(engine->setting()));
	adapter->activate();
	engine->waitForShutdown();
	return 0;
}

int main(int argc, char **argv)
{
	urandom_has_device();	// use it to seed the random() function
	SettingPtr setting = newSetting();
	setting->insert("Fcgi.ThreadPoolNumber", "2");
	setting->insert("xic.PThreadPool.Server.SizeMax", "2");
	return xic::start_xic_pt(run, argc, argv, setting);
}

/* 
 * vim: set ts=8 sw=8 noet:
 */
