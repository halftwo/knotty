#include "EngineImp.h"
#include "XicException.h"
#include "dlog/dlog.h"
#include "xslib/uuid.h"
#include "xslib/bit.h"
#include "xslib/cstr.h"
#include "xslib/hex.h"
#include "xslib/xnet.h"
#include "xslib/ostk.h"
#include "xslib/xstr.h"
#include "xslib/cxxstr.h"
#include "xslib/rdtsc.h"
#include "xslib/cpu.h"
#include "xslib/xlog.h"
#include "xslib/unix_user.h"
#include "xslib/urandom.h"
#include "xslib/crc64.h"
#include "xslib/crc.h"
#include "xslib/ScopeGuard.h"
#include "xslib/Enforce.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <limits.h>
#include <errno.h>
#include <string>
#include <sstream>

#define DEFAULT_CLOSE_TIMEOUT	(900*1000)	/* milliseconds */
#define DEFAULT_CONNECT_TIMEOUT	(60*1000)	/* milliseconds */

#define SUICIDE_DOOM_MIN	3		/* in seconds */
#define SUICIDE_DOOM_MAX	300		/* in seconds */

#define MESSAGE_SIZE_LOWER	4096
#define MESSAGE_SIZE_DEFAULT	(64*1024*1024)
#define MESSAGE_SIZE_UPPER	INT32_MAX
#define DEFAULT_ACM_SERVER	0
#define DEFAULT_ACM_CLIENT	300
#define MAX_SAMPLE		INT_MAX

#define CHUNK_SIZE		4096

using namespace xic;


const CompletionPtr xic::NULL_COMPLETION = CompletionPtr();
const AnswerPtr xic::ONEWAY_ANSWER = AnswerPtr();
const AnswerPtr xic::ASYNC_ANSWER = Answer::create();

uint32_t xic::xic_message_size = MESSAGE_SIZE_DEFAULT;

bool xic::xic_dlog_sq;
bool xic::xic_dlog_sa;
bool xic::xic_dlog_sae;

bool xic::xic_dlog_cq;
bool xic::xic_dlog_ca;
bool xic::xic_dlog_cae;

bool xic::xic_dlog_warning = true;
bool xic::xic_dlog_debug;

int xic::xic_timeout_connect;
int xic::xic_timeout_close;
int xic::xic_timeout_message;

int xic::xic_acm_server = DEFAULT_ACM_SERVER;
int xic::xic_acm_client = DEFAULT_ACM_CLIENT;

int xic::xic_sample_server = 0;
int xic::xic_sample_client = 0;

int xic::xic_slow_server = -1;
int xic::xic_slow_client = -1;

bool xic::xic_except_server = false;
bool xic::xic_except_client = false;

IpMatcherPtr xic::xic_allow_ips;
SecretBoxPtr xic::xic_passport_secret;
ShadowBoxPtr xic::xic_passport_shadow;

EnginePtr xic::xic_engine;

static int64_t slow_srv_tsc = -1;
static int64_t slow_cli_tsc = -1;

static char locus_srv_slow[24];
static char locus_cli_slow[24];
static char locus_srv_sample[24];
static char locus_cli_sample[24];

static bool _ready_to_serve;


static void prepare_locus_slow()
{
	snprintf(locus_srv_slow, sizeof(locus_srv_slow) - 1, "/SLOW:%d", xic_slow_server);
	snprintf(locus_cli_slow, sizeof(locus_cli_slow) - 1, "/SLOW:%d", xic_slow_client);
	adjustTSC(cpu_frequency());
}

static void prepare_locus_sample()
{
	snprintf(locus_srv_sample, sizeof(locus_srv_sample) - 1, "/SAMPLE:%d", xic_sample_server);
	snprintf(locus_cli_sample, sizeof(locus_cli_sample) - 1, "/SAMPLE:%d", xic_sample_client);
}

size_t xic::cli_sample_locus(char *buf)
{
	char *p = stpcpy(buf, locus_cli_sample);
	return p - buf;
}

void xic::adjustTSC(uint64_t frequency)
{
	slow_srv_tsc = xic_slow_server * frequency / 1000;
	slow_cli_tsc = xic_slow_client * frequency / 1000;
}

struct iovec *xic::get_msg_iovec(const XicMessagePtr& msg, int *count)
{
	int iov_count = 0;
	struct iovec *body_iov = msg->body_iovec(&iov_count);

	ostk_t *ostk = msg->ostk();
	struct iovec *iov = OSTK_ALLOC(ostk, struct iovec, iov_count + 2);
	memcpy(&iov[1], body_iov, iov_count * sizeof(struct iovec));

	XicMessage::Header *hdr = OSTK_ALLOC_ONE(ostk, XicMessage::Header);
	hdr->magic = 'X';
	hdr->version = 'I';
	hdr->msgType = msg->msgType();
	hdr->flags = 0;
	hdr->bodySize = xnet_m32(msg->bodySize());

	iov[0].iov_base = hdr;
	iov[0].iov_len = sizeof(XicMessage::Header);
	++iov_count;
	*count = iov_count;
	return iov;
}

void xic::free_msg_iovec(const XicMessagePtr& msg, struct iovec *iov)
{
	ostk_free(msg->ostk(), iov);
}

class PassThroughCompletion: public Completion
{
	WaiterPtr _waiter;
public:
	PassThroughCompletion(const WaiterPtr& waiter)
		: _waiter(waiter)
	{
	}

	virtual void completed(const ResultPtr& result)
	{
		_waiter->response(result->takeAnswer(false), true);
	}
};

CompletionPtr Completion::createPassThrough(const WaiterPtr& waiter)
{
	return CompletionPtr(new PassThroughCompletion(waiter));
}

void Proxy::requestOneway(const QuestPtr& quest)
{
	if (quest->txid())
		throw XERROR_MSG(XLogicError, "Using twoway quest to call Proxy::requestOneway()");
	emitQuest(quest, NULL_COMPLETION);
}

AnswerPtr Proxy::request(const QuestPtr& quest)
{
	if (!quest->txid())
		throw XERROR_MSG(XLogicError, "Using oneway quest to call Proxy::request()");
	ResultPtr result = emitQuest(quest, NULL_COMPLETION);
	return result->takeAnswer(true);
}

static void _xlog_writer(int level, const char *locus, const char *buf, size_t size)
{
	char tag[32];
	char *p = cstr_copy(tag, "XLOG.");
	p += cstr_from_long(p, level > 0 ? level : 0);
	xstr_t tag_xs = XSTR_INIT((unsigned char *)tag, (p - tag));
	xstr_t locus_xs = XSTR_C(locus ? locus : "-");
	xstr_t content_xs = XSTR_INIT((unsigned char *)buf, (ssize_t)size);
	zdlog(NULL, &tag_xs, &locus_xs, &content_xs);

	if (!_ready_to_serve)
		xlog_default_writer(level, locus, buf, size);
}

static void _set_rlimit(int resource, rlim_t n)
{
	struct rlimit rlim;
	int rc = getrlimit(resource, &rlim);
	rlim.rlim_cur = n;
	if (rc != 0 || (rlim.rlim_max != RLIM_INFINITY && rlim.rlim_max < n))
		rlim.rlim_max = n;

	rc = setrlimit(resource, &rlim);
	if (rc < 0 && xic_dlog_warning)
	{
		dlog("XIC.WARNING", "setrlimit(%d, %jd) failed, errno=%d %m", resource, (intmax_t)n, errno);
	}
}

static void _tune_up_hard_nofile(rlim_t n)
{
	if (geteuid() == 0)
	{
		struct rlimit rlim;
		int rc = getrlimit(RLIMIT_NOFILE, &rlim);
		if (rc < 0)
		{
			if (xic_dlog_warning)
				dlog("XIC.WARNING", "getrlimit() failed, errno=%d %m", errno);
		}
		else if (rlim.rlim_max != RLIM_INFINITY && rlim.rlim_max < n)
		{
			rlim.rlim_max = n;
			rc = setrlimit(RLIMIT_NOFILE, &rlim);
			if (rc < 0 && xic_dlog_warning)
			{
				dlog("XIC.WARNING", "setrlimit() failed, errno=%d %m", errno);
			}
		}
	}
}

static int _vbs_string_payload_abbr_print(iobuf_t *ob, const xstr_t *xs, bool is_blob)
{
	int max = is_blob ? 256 : 512;
	if (xs->len <= max)
	{
		return vbs_string_payload_default_print(ob, xs, is_blob);
	}

	xstr_t s = XSTR_INIT(xs->data, max/3*2);
	if (vbs_string_payload_default_print(ob, &s, is_blob) < 0)
		return -1;

	if (iobuf_write(ob, "~~~", 3) != 3)
		return -1;

	s.len = max/3;
	s.data += xs->len - s.len;
	if (vbs_string_payload_default_print(ob, &s, is_blob) < 0)
		return -1;
	return 0;
}

void xic::prepareEngine(const SettingPtr& setting)
{
	static bool done = false;

	vbs_set_string_payload_print_function(_vbs_string_payload_abbr_print);
	xlog_set_writer(_xlog_writer);

	_tune_up_hard_nofile(65536);

	if (!done && setting)
	{
		done = true;
		get_cpu_frequency(0);

		int mask = setting->getInt("xic.umask", -1);
		if (mask >= 0)
			umask(mask);

		cpu_alignment_check(setting->getBool("xic.cpu.ac", false));

		xic_allow_ips = new IpMatcher(setting->getString("xic.allow.ips"));

		std::string s;
		s = setting->getString("xic.passport.secret");
		if (!s.empty())
			xic_passport_secret = SecretBox::createFromFile(s);

		s = setting->getString("xic.passport.shadow");
		if (!s.empty())
			xic_passport_shadow = ShadowBox::createFromFile(s);

		xic_acm_server = setting->getInt("xic.acm.server", DEFAULT_ACM_SERVER);
		xic_acm_client = setting->getInt("xic.acm.client", DEFAULT_ACM_CLIENT);

		intmax_t n;
		n = setting->getInt("xic.message.size", MESSAGE_SIZE_DEFAULT);
		xic_message_size = XS_CLAMP(n, MESSAGE_SIZE_LOWER, MESSAGE_SIZE_UPPER);

		xlog_level = setting->getInt("xic.xlog.level", xlog_level);
		XError::how = setting->getInt("xic.XError.how", XError::how);

		std::string id = setting->getString("xic.dlog.identity");
		if (!id.empty())
			dlog_set(id.c_str(), 0);

		xic_dlog_sq = setting->getBool("xic.dlog.sq");
		xic_dlog_sa = setting->getBool("xic.dlog.sa");
		xic_dlog_sae = setting->getBool("xic.dlog.sae");

		xic_dlog_cq = setting->getBool("xic.dlog.cq");
		xic_dlog_ca = setting->getBool("xic.dlog.ca");
		xic_dlog_cae = setting->getBool("xic.dlog.cae");

		xic_timeout_connect = setting->getInt("xic.timeout.connect");
		xic_timeout_close = setting->getInt("xic.timeout.close");
		xic_timeout_message = setting->getInt("xic.timeout.message");

		n = setting->getInt("xic.slow.server", INTMAX_MIN);
		if (n != INTMAX_MIN)
		{
			xic_slow_server = n < 0 ? -1 : n < INT_MAX ? n : INT_MAX;
		}

		n = setting->getInt("xic.slow.client", INTMAX_MIN);
		if (n != INTMAX_MIN)
		{
			xic_slow_client = n < 0 ? -1 : n < INT_MAX ? n : INT_MAX;
		}

		prepare_locus_slow();

		n = setting->getInt("xic.sample.server");
		xic_sample_server = XS_CLAMP(n, 0, MAX_SAMPLE);

		n = setting->getInt("xic.sample.client");
		xic_sample_client = XS_CLAMP(n, 0, MAX_SAMPLE);

		prepare_locus_sample();

		xic_except_server = setting->getBool("xic.except.server");
		xic_except_client = setting->getBool("xic.except.client");

		n = setting->getInt("xic.rlimit.core", INTMAX_MIN);
		if (n != INTMAX_MIN)
		{
			if (n == -1)
				 n = RLIM_INFINITY;

			_set_rlimit(RLIMIT_CORE, n);
		}

		n = setting->getInt("xic.rlimit.nofile", INTMAX_MIN);
		if (n != INTMAX_MIN)
		{
			if (n == -1)
				 n = RLIM_INFINITY;

			_set_rlimit(RLIMIT_NOFILE, n);
		}

		n = setting->getInt("xic.rlimit.as", INTMAX_MIN);
		if (n != INTMAX_MIN)
		{
			if (n == -1)
				 n = RLIM_INFINITY;

			_set_rlimit(RLIMIT_AS, n);
		}

		xic_dlog_debug = setting->getBool("xic.dlog.debug");
		xic_dlog_warning = setting->getBool("xic.dlog.warning", true);
	}
}

void xic::readyToServe(const SettingPtr& setting)
{
	_ready_to_serve = true;

	std::string user = setting->getString("xic.user");
	std::string group = setting->getString("xic.group");
	if (!user.empty() || !group.empty())
		unix_set_user_group(user.c_str(), group.c_str());
}

void xic::parseEndpoint(const xstr_t& endpoint, xic::EndpointInfo& ei)
{
	xstr_t xs = endpoint;
	xstr_t netloc;

	xstr_token_bset(&xs, &blank_bset, &netloc);
	if (!xstr_delimit_char(&netloc, '+', &ei.proto) || !xstr_delimit_char(&netloc, '+', &ei.host) || netloc.data == NULL)
		throw XERROR_FMT(EndpointParseException, "Invalid format. endpoint=%.*s", XSTR_P(&endpoint));

	if (ei.proto.len == 0 || xstr_case_equal_cstr(&ei.proto, "tcp"))
		xstr_const(&ei.proto, "tcp");
	else
		throw XERROR_FMT(XError, "Unsupported transport protocol '%.*s'", XSTR_P(&ei.proto));

	xstr_t port_xs = netloc;
	xstr_t port_end;
	int pt = xstr_to_long(&port_xs, &port_end, 10);

	if (pt < 0 || pt > 65535 || port_end.len)
		throw XERROR_FMT(EndpointParseException, "Invalid port number '%.*s'", XSTR_P(&port_xs));

	ei.port = pt;

	xstr_t value;
	while (xstr_token_bset(&xs, &blank_bset, &value))
	{
		xstr_t key;
		xstr_delimit_char(&value, '=', &key);
		if (xstr_equal_cstr(&key, "timeout"))
		{
			xstr_t tmp;

			xstr_delimit_char(&value, ',', &tmp);
			ei.timeout = xstr_to_long(&tmp, NULL, 10);
			if (ei.timeout < 0)
				ei.timeout = 0;

			xstr_delimit_char(&value, ',', &tmp);
			ei.close_timeout = xstr_to_long(&tmp, NULL, 10);
			if (ei.close_timeout < 0)
				ei.close_timeout = 0;

			xstr_delimit_char(&value, ',', &tmp);
			ei.connect_timeout = xstr_to_long(&tmp, NULL, 10);
			if (ei.connect_timeout < 0)
				ei.connect_timeout = 0;
		}
	}
}

size_t xic::getIps(const xstr_t& host, uint32_t ipv4s[], int *v4num, uint8_t ipv6s[][16], int *v6num, bool& any)
{
	assert(*v4num > 0 || *v6num > 0);
	any = false;

	char ipstr[128];
	int pos = xstr_find_char(&host, 0, '/');
	bool is_ipv6 = (xstr_rfind_char(&host, pos, ':') > 0);
	int prefix_len = -1;
	if (pos > 0)
	{
		xstr_slice_copy_to(&host, 0, pos, ipstr, sizeof(ipstr));
		xstr_t tmp = xstr_suffix(&host, pos + 1);
		prefix_len = xstr_atoi(&tmp);
		if (prefix_len == 0)
			prefix_len = -1;
		else if (is_ipv6 && prefix_len >= 128)
			prefix_len = -1;
		else if (prefix_len >= 32)
			prefix_len = -1;
	}
	else
	{
		xstr_copy_to(&host, ipstr, sizeof(ipstr));
	}

	if (ipstr[0] == 0 || strcmp(ipstr, "::") == 0)
	{
		int n = xnet_get_all_ips(ipv6s, v6num, ipv4s, v4num); 
		if (n < 0)
			throw XERROR_FMT(XError, "xnet_get_all_ips() failed, errno=%d", errno);
		else if (n == 0)
			return 0;

		any = true;
		return *v4num + *v6num;
	}

	uint8_t the_ipv6[16];
	uint32_t the_ipv4;
	if (is_ipv6)
	{
		*v4num = 0;
		if (*v6num <= 0)
			return 0;

		if (strcmp(ipstr, "localhost") == 0)
		{
			memcpy(the_ipv6, &in6addr_loopback, 16);
			prefix_len = -1;
		}
		else if (!xnet_ipv6_aton(ipstr, the_ipv6))
		{
			*v6num = 0;
			return 0;
		}
	}
	else
	{
		*v6num = 0;
		if (*v4num <= 0)
			return 0;

		if (strcmp(ipstr, "localhost") == 0)
		{
			the_ipv4 = 0x7F000001;
			prefix_len = -1;
		}
		else if (!xnet_ipv4_aton(ipstr, &the_ipv4))
		{
			*v4num = 0;
			return 0;
		}
	}

	if (prefix_len < 0)
	{
		if (is_ipv6)
		{
			memcpy(ipv6s[0], the_ipv6, 16);
			*v6num = 1;
		}
		else
		{
			ipv4s[0] = the_ipv4;
			*v4num = 1;
		}
		return 1;
	}

	int n = xnet_get_all_ips(ipv6s, v6num, ipv4s, v4num); 
	if (n < 0)
		throw XERROR_FMT(XError, "xnet_get_all_ips() failed, errno=%d", errno);

	if (is_ipv6)
	{
		int k = 0, num = *v6num;
		for (int i = 0; i < num; ++i)
		{
			if (bitmap_msb_equal(ipv6s[i], the_ipv6, prefix_len))
			{
				if (i > k)
					memmove(&ipv6s[k], &ipv6s[i], (num - i)*16);
				++k;
			}
		}
		*v6num = k;
		return k;
	}
	else
	{
		int k = 0, num = *v4num;
		for (int i = 0; i < num; ++i)
		{
			if (bit_msb32_equal(ipv4s[i], the_ipv4, prefix_len))
			{
				if (i > k)
					memmove(&ipv4s[k], &ipv4s[i], (num - i)*sizeof(uint32_t));
				++k;
			}
		}
		*v4num = k;
		return k;
	}
}

static std::string make_raiser(const xstr_t& method, const xstr_t& service, const std::string& endpoint)
{
	std::string s;
	s.reserve(method.len + service.len + endpoint.length() + 3);
	s.append((const char *)method.data, method.len);
	s.push_back('*');
	s.append((const char *) service.data, service.len);
	s.append(" @", 2);
	s.append(endpoint);
	return s;
}

static AnswerPtr append_raiser(const AnswerPtr& answer, const std::string& raiser)
{
	assert(answer->status());
	AnswerWriter aw(AnswerWriter::EXCEPTION);
	bool done = false;
	const vbs_dict_t *ps = answer->args_dict();
	for (const vbs_ditem_t *ent = ps->first; ent; ent = ent->next)
	{
		const xstr_t& key = ent->key.d_xstr;
		if (xstr_equal_cstr(&key, "raiser") && ent->value.type == VBS_STRING)
		{
			const xstr_t& v = ent->value.d_xstr;
			aw.paramStrHead("raiser", v.len + 2 + raiser.length());
			aw.raw(v.data, v.len);
			aw.raw(", ", 2);
			aw.raw(raiser.data(), raiser.length());
			done = true;
		}
		else if (!xstr_equal_cstr(&key, "_local"))
		{
			aw.param(key, ent->value);
		}
	}

	if (!done)
	{
		static const xstr_t unknown = XSTR_CONST("UNKNOWN_RAISER, ");
		aw.paramStrHead("raiser", unknown.len + raiser.length());
		aw.raw(unknown.data, unknown.len);
		aw.raw(raiser.data(), raiser.length());
	}

	return aw.take();
}

/*
   Exception answer should cantain following fields:
	exception	%S	STRING
	code		%i	INTEGER
	tag		%S	STRING
	message		%S	STRING
	raiser		%S	STRING
	detail		{%S^%X}	DICT

   If the exception is local generated, the answer should contain field 
   "_local" and set its value to boolean true. 
 */
AnswerPtr xic::except2answer(const std::exception& ex, const xstr_t& method, const xstr_t& service,
			const std::string& endpoint, bool local)
{
	AnswerWriter aw(AnswerWriter::EXCEPTION);

	const XError* e = dynamic_cast<const XError*>(&ex);
	const xic::RemoteException* re = e ? dynamic_cast<const xic::RemoteException*>(&ex) : NULL;
	if (re)
	{
		std::string raiser = re->raiser();
		if (raiser.empty())
			raiser = "UNKNOWN_RAISER, " + make_raiser(method, service, endpoint);
		else
			raiser += ", " + make_raiser(method, service, endpoint);
		aw.param("exname", re->exname());
		aw.param("code", re->code());
		aw.param("tag", re->tag());
		aw.param("message", re->message());
		aw.param("raiser", raiser);
		aw.param("detail", re->detail());
	}
	else
	{
		if (e)
		{
			aw.param("exname", e->exname());
			aw.param("code", e->code());
			aw.param("tag", e->tag());
			aw.param("message", e->message());
		}
		else
		{
			const char *name = typeid(ex).name();
			char *demangled = demangle_cxxname(name, NULL, NULL);
			if (demangled)
			{
				aw.param("exname", demangled);
				free(demangled);
			}
			else
			{
				aw.param("exname", name);
			}

			aw.param("code", 0);
			aw.param("tag", "");
			aw.param("message", ex.what());
		}

		aw.param("raiser", make_raiser(method, service, endpoint));
		if (local)
			aw.param("_local", true);

		VDictWriter dw = aw.paramVDict("detail", 0);
		dw.kv("what", ex.what());
		if (e)
		{
			dw.kv("file", e->file());
			dw.kv("line", e->line());
			dw.kv("calltrace", e->calltrace());
		}
	}

	return aw.take();
}

HelloMessagePtr HelloMessage::create()
{
	ostk_t *ostk = ostk_create(CHUNK_SIZE);
	void *p = ostk_alloc(ostk, sizeof(HelloMessage));
	return new(p) HelloMessage(ostk);
}

HelloMessage::HelloMessage(ostk_t *ostk)
{
	_ostk = ostk;
	_msgType = 'H';
}

struct iovec* HelloMessage::body_iovec(int* count)
{
	*count = _iov_count;
	return _iov;
}

ByeMessagePtr ByeMessage::create()
{
	ostk_t *ostk = ostk_create(CHUNK_SIZE);
	void *p = ostk_alloc(ostk, sizeof(ByeMessage));
	return new(p) ByeMessage(ostk);
}

ByeMessage::ByeMessage(ostk_t *ostk)
{
	_ostk = ostk;
	_msgType = 'B';
}

struct iovec* ByeMessage::body_iovec(int* count)
{
	*count = _iov_count;
	return _iov;
}


ResultMap::ResultMap(): _next_id(1)
{
}

int64_t ResultMap::generateId()
{
	if (_next_id <= 0)
		_next_id = 1;
	return _next_id++;
}

int64_t ResultMap::addResult(const ResultIPtr& result)
{
	int64_t txid;
	bool r;
	do
	{
		txid = generateId();
		r = addResult(txid, result);
	} while (!r);
	return txid;
}

bool ResultMap::addResult(int64_t txid, const ResultIPtr& result)
{
	std::map<int64_t, ResultIPtr>::iterator iter = _map.insert(_map.end(), std::make_pair(txid, result));
	return iter != _map.end();
}

ResultIPtr ResultMap::removeResult(int64_t txid)
{
	ResultIPtr res;
	std::map<int64_t, ResultIPtr>::iterator iter = _map.find(txid);
	if (iter != _map.end())
	{
		res = iter->second;
		_map.erase(iter);
	}
	return res;
}

ResultIPtr ResultMap::findResult(int64_t txid) const
{
	std::map<int64_t, ResultIPtr>::const_iterator iter = _map.find(txid);
	if (iter != _map.end())
		return iter->second;
	return ResultIPtr();
}

ProxyI::ProxyI(const std::string& service, EngineI* engine, ConnectionI* con)
	: _engine(engine), _proxy(service), _idx(0), _loadBalance(LB_NORMAL), _incoming(false)
{
	if (con)
	{
		_incoming = true;
		_service = _proxy;
		_cons.push_back(ConnectionIPtr(con));
		return;
	}

	xstr_t tmp = make_xstr(_proxy);
	xstr_t so, svc;
	xstr_delimit_char(&tmp, '@', &so);
	xstr_token_space(&so, &svc);
	if (svc.len == 0)
		throw XERROR_FMT(XArgumentError, "invalid proxy(%s)", _proxy.c_str());
	_service = make_string(svc);

	xstr_t option;
	while (xstr_token_space(&so, &option))
	{
		if (xstr_equal_cstr(&option, "-lb:random"))
		{
			_loadBalance = LB_RANDOM;
		}
		else if (xstr_equal_cstr(&option, "-lb:hash"))
		{
			_loadBalance = LB_HASH;
		}
		else if (xstr_equal_cstr(&option, "-lb:normal"))
		{
			_loadBalance = LB_NORMAL;
		}
	}

	xstr_t endpoint;
	while (xstr_delimit_char(&tmp, '@', &endpoint))
	{
		xstr_trim(&endpoint);
		if (endpoint.len)
			_endpoints.push_back(make_string(endpoint));
	}

	if (_endpoints.empty())
		throw XERROR_FMT(XArgumentError, "invalid proxy(%s), no endpoint", _proxy.c_str());

	_cons.resize(_endpoints.size());

	if (_loadBalance == LB_HASH)
	{
		std::vector<uint64_t> members;
		members.reserve(_endpoints.size());
		for (size_t i = 0; i < _endpoints.size(); ++i)
		{
			const std::string& ep = _endpoints[i];
			members.push_back(crc64_checksum(ep.data(), ep.length()));
		}
		_cseq.reset(new CarpSequence(&members[0], _endpoints.size(), 0xFFFF));
		_cseq->enable_cache();
	}
}

ProxyI::~ProxyI()
{
	for (size_t i = 0; i < _cons.size(); ++i)
	{
		if (_cons[i])
			_cons[i]->disconnect();
	}
}

ConnectionIPtr ProxyI::pickConnection(const QuestPtr& quest)
{
	ConnectionIPtr con;
	Connection::State st;

	if (_loadBalance == LB_RANDOM)
	{
		size_t k = urandom_get_int(0, _cons.size());
		con = _cons[k];
		st = con ? con->state() : Connection::ST_INIT;
		if (!con || isGraceful(st))
		{
			_cons[k] = _engine->makeConnection(_service, _endpoints[k]);
			con = _cons[k];
		}
		else if (!isActive(st) && !(isLive(st) && con->attempt() < 1))
		{
			if (!isLive(st) && con->ex_time() < _engine->time() - 3)
				_cons[k] = _engine->makeConnection(_service, _endpoints[k]);

			for (size_t i = 0; i < _endpoints.size(); ++i)
			{
				++k;
				if (k >= _endpoints.size())
					k = 0;
				con = _cons[k];
				st = con ? con->state() : Connection::ST_INIT;
				if (!con)
				{
					_cons[k] = _engine->makeConnection(_service, _endpoints[k]);
					con = _cons[k];
					break;
				}
				else if (isActive(st) || (isLive(st) && con->attempt() < 1))
				{
					break;
				}
			}
		}
	}
	else if (_loadBalance == LB_HASH)
	{
		VDict ctx = quest->context();
		const vbs_data_t *ht = ctx.get_data("XIC_HINT");
		intmax_t hint = 0;
		if (ht)
		{
			if (ht->type == VBS_INTEGER)
				hint = ht->d_int;
			else if (ht->type == VBS_STRING || ht->type == VBS_BLOB)
				hint = crc32_checksum(ht->d_blob.data, ht->d_blob.len);
			else if (ht->type == VBS_FLOATING)
				hint = ht->d_floating;
			else if (ht->type == VBS_DECIMAL)
				decimal64_to_integer(ht->d_decimal64, &hint);
			else
				ht = NULL;
		}

		if (!ht && xic_dlog_warning)
		{
			const xstr_t& service = quest->service();
			const xstr_t& method = quest->method();
			dlog("XIC.WARNING", "XIC_HINT invalid or not specified in context, Q=%.*s::%.*s C%p{>VBS_DICT<}",
				XSTR_P(&service), XSTR_P(&method), ctx.dict());
		}

		int k = _cseq->which((uint32_t)hint);
		con = _cons[k];
		st = con ? con->state() : Connection::ST_INIT;
		if (!con || isGraceful(st))
		{
			_cons[k] = _engine->makeConnection(_service, _endpoints[k]);
			con = _cons[k];
		}
		else if (!isActive(st) && !(isLive(st) && con->attempt() < 1))
		{
			if (!isLive(st) && con->ex_time() < _engine->time() - 3)
				_cons[k] = _engine->makeConnection(_service, _endpoints[k]);

			int seqs[5];
			int n = _cseq->sequence(hint, seqs, 5);
			for (int i = 1; i < n; ++i)
			{
				k = seqs[i];
				con = _cons[k];
				st = con ? con->state() : Connection::ST_INIT;
				if (!con)
				{
					_cons[k] = _engine->makeConnection(_service, _endpoints[k]);
					con = _cons[k];
					break;
				}
				else if (isActive(st) || (isLive(st) && con->attempt() < 1))
				{
					break;
				}
			}
		}
	}
	else /* LB_NORMAL */
	{
		con = _cons[_idx];
		if (!con || !isLive(con->state()))
		{
			++_idx;
			if (_idx >= (int)_endpoints.size())
			{
				_idx = 0;
			}
			con = _engine->makeConnection(_service, _endpoints[_idx]);
			_cons[_idx] = con;
		}
	}

	return con;
}

ProxyPtr ProxyI::timedProxy(int timeout, int close_timeout, int connect_timeout) const
{
	size_t size = _endpoints.size();
	if (size == 0)
		throw XERROR(ProxyFixedException);

	std::ostringstream os;
	os << _service;

	for (size_t i = 0; i < size; ++i)
	{
		xstr_t xs = XSTR_CXX(_endpoints[i]);
		xstr_t netloc, value;
		xstr_token_bset(&xs, &blank_bset, &netloc);
		os << " @" << netloc;

		while (xstr_token_bset(&xs, &blank_bset, &value))
		{
			xstr_t key;
			xstr_delimit_char(&value, '=', &key);
			if (xstr_equal_cstr(&key, "timeout"))
				continue;

			os << ' ' << value;
		}

		if (timeout > 0 || connect_timeout > 0 || close_timeout > 0)
		{
			os << " timeout=" << (timeout > 0 ? timeout : 0);
			os << ',' << (close_timeout > 0 ? close_timeout : 0);
			os << ',' << (connect_timeout > 0 ? connect_timeout : 0);
		}
	}

	return _engine->stringToProxy(os.str());
}

ConnectionI::ConnectionI(bool incoming, int attempt, int timeout, int close_timeout, int connect_timeout)
	: _graceful(false), _incoming(incoming), _msg_timeout(timeout), _close_timeout(close_timeout), 
	_connect_timeout(connect_timeout), _attempt(attempt)
{
	_ex_time = 0;
	_state = ST_INIT;
	_ck_state = CK_INIT;
	_sock_ip[0] = 0;
	_sock_port = 0;
	_peer_ip[0] = 0;
	_peer_port = 0;
	_processing = 0;
}

int ConnectionI::closeTimeout()
{
	return (_close_timeout > 0) ? _close_timeout
		: (xic_timeout_close > 0) ? xic_timeout_close
		: (_msg_timeout > 0) ? _msg_timeout
		: (xic_timeout_message > 0) ? xic_timeout_message
		: DEFAULT_CLOSE_TIMEOUT;
}

int ConnectionI::connectTimeout()
{
	return (_connect_timeout > 0) ? _connect_timeout
		: (xic_timeout_connect > 0) ? xic_timeout_connect
		: (_msg_timeout > 0) ? _msg_timeout
		: (xic_timeout_message > 0) ? xic_timeout_message
		: DEFAULT_CONNECT_TIMEOUT;
}

void ConnectionI::handle_quest(CurrentI& current)
{
	Adapter* adapter = current.adapter.get();
	Quest* q = current._quest.get();
	int64_t txid = current._txid;
	const xstr_t& service = current._service;
	const xstr_t& method = current._method;
	AnswerPtr answer;
	bool trace = true;

	try
	{
		if (xic_dlog_sq)
		{
			xdlog(vbs_xfmt, NULL, "XIC.SQ", "/",
				"%u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_DICT<} %p{>VBS_DICT<}",
				_sock_port, _peer_ip, _peer_port,
				(intmax_t)txid, XSTR_P(&service), XSTR_P(&method), 
				q->context_dict(), q->args_dict());
		}

		ServantPtr srv;
		if (adapter && service.len && method.len)
		{
			srv = adapter->findServant(make_string(service));
			if (!srv)
			{
				srv = adapter->getDefaultServant();
				if (!srv)
					throw XERROR_FMT(ServiceNotFoundException, "%.*s", XSTR_P(&service));
			}
		}
		else
		{
			if (service.len == 0)
				throw XERROR(ServiceEmptyException);

			if (method.len == 0)
				throw XERROR(MethodEmptyException);

			if (!adapter)
				throw XERROR(AdapterAbsentException);
		}

		answer = srv->process(current._quest, current);

		if (txid)
		{
			if (!answer && !current._waiter)
				throw XERROR(MethodOnewayException);

			if (answer == ASYNC_ANSWER)
			{
				answer.reset();
				if (!current._waiter)
					throw XERROR_MSG(ServantException, "No answer would be returned from servant");
			}
		}
		else
		{
			if (answer == ASYNC_ANSWER)
				answer.reset();
		}
	}
	catch (std::exception& ex)
	{
		if (txid && !answer)
		{
			answer = except2answer(ex, method, service, _endpoint);
			trace = false;
			if (xic_dlog_warning && dynamic_cast<xic::XicException*>(&ex))
			{
				dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s exception=%s",
					_peer_ip, _peer_port, XSTR_P(&service), XSTR_P(&method), ex.what());
			}
		}
		else if (xic_dlog_warning)
		{
			dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s exception=%s",
				_peer_ip, _peer_port, XSTR_P(&service), XSTR_P(&method), ex.what());
		}
	}

	if (current._waiter)
	{
		if (answer)
		{
			if (xic_dlog_warning && !answer->status())
			{
				dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s #=Servant::process() function called"
					" Current::asynchronous() and returned an answer simultaneously",
					_peer_ip, _peer_port, XSTR_P(&service), XSTR_P(&method));
			}
			current._waiter->response(answer, trace);
		}
		return;
	}

	int64_t used_tsc = 0;
	int status = answer ? answer->status() : 0;
	bool log_answer = (answer && (xic_dlog_sa || (status && xic_dlog_sae)));
	bool log_slow = (slow_srv_tsc >= 0 && (used_tsc = rdtsc() - current._start_tsc) > slow_srv_tsc);
	bool log_sample = (xic_sample_server > 0 && (xic_sample_server == 1 
				|| (random() / (RAND_MAX + 1.0) * xic_sample_server) < 1));
	bool log_except = xic_except_server && status;
	bool log_mark = current._logit;

	if (log_answer || log_slow || log_sample || log_except || log_mark)
	{
		if (!used_tsc)
			used_tsc = rdtsc() - current._start_tsc;
		int64_t used_ms = used_tsc * 1000 / cpu_frequency();

		if (log_answer)
		{
			if (status && !xic_dlog_sq)
			{
				xdlog(vbs_xfmt, NULL, "XIC.SQE", "/S/",
					"%u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_DICT<} %p{>VBS_DICT<}",
					_sock_port, _peer_ip, _peer_port,
					(intmax_t)txid, XSTR_P(&service), XSTR_P(&method), 
					q->context_dict(), q->args_dict());
			}

			const char *tag = !txid ? "XIC.SAX" : status ? "XIC.SAE" : "XIC.SAN";
			xdlog(vbs_xfmt, NULL, tag, "/S/",
				"%u/%s+%u %jd Q=%.*s::%.*s T=%d.%03d A=%d %p{>VBS_RAW<}",
				_sock_port, _peer_ip, _peer_port,
				(intmax_t)txid, XSTR_P(&service), XSTR_P(&method),
				(int)(used_ms / 1000), (int)(used_ms % 1000),
				status, &answer->args_xstr());
		}

		if (log_slow || log_sample || log_except || log_mark)
		{
			char locus[64];
			char *p = locus;
			if (log_slow)
				p = stpcpy(p, locus_srv_slow);
			if (log_sample)
				p = stpcpy(p, locus_srv_sample);
			if (log_except)
				p = stpcpy(p, "/EXCEPT");
			if (log_mark)
				p = stpcpy(p, "/MARK");
			*p++ = '/';
			*p++ = 'S';
			*p++ = '/';
			*p = 0;

			if (answer)
			{
				xdlog(vbs_xfmt, NULL, "XIC.SQA", locus,
					"T=%d.%03d %u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_DICT<} %p{>VBS_DICT<} A=%d %p{>VBS_RAW<}",
					(int)(used_ms / 1000), (int)(used_ms % 1000),
					_sock_port, _peer_ip, _peer_port,
					(intmax_t)txid, XSTR_P(&service), XSTR_P(&method), 
					q->context_dict(), q->args_dict(),
					status, &answer->args_xstr());
			}
			else
			{
				xdlog(vbs_xfmt, NULL, "XIC.SQA", locus,
					"T=%d.%03d %u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_DICT<} %p{>VBS_DICT<} A=(null)",
					(int)(used_ms / 1000), (int)(used_ms % 1000),
					_sock_port, _peer_ip, _peer_port,
					(intmax_t)txid, XSTR_P(&service), XSTR_P(&method), 
					q->context_dict(), q->args_dict());
			}
		}
	}

	if (answer)
	{
		if (txid)
		{
			answer->setTxid(txid);

			// NB: The check for message size should be after calling answer->setTxid().
			if (answer->bodySize() > xic_message_size)
			{
				XERROR_VAR_FMT(ServantException, ex,
					"Huge sized answer responded from servant, size=%u>%u",
					answer->bodySize(), xic_message_size);

				if (xic_dlog_warning)
				{
					dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s exception=%s",
						_peer_ip, _peer_port, XSTR_P(&service), XSTR_P(&method), ex.what());
				}
				answer = except2answer(ex, method, service, endpoint());
				answer->setTxid(txid);
			}

			replyAnswer(answer);
		}
		else if (xic_dlog_warning)
		{
			dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s #=answer for oneway quest discarded",
				_peer_ip, _peer_port, XSTR_P(&service), XSTR_P(&method));
		}
	}
}

void ConnectionI::handle_answer(AnswerPtr& answer, const ResultIPtr& result)
{
	ResultI* res = result.get();
	int status = answer->status();
	int64_t used_tsc = 0;
	bool log_answer = (!res || xic_dlog_ca || (status && xic_dlog_cae));
	bool log_slow = (res && slow_cli_tsc >= 0 && (used_tsc = rdtsc() - res->start_tsc()) > slow_cli_tsc);
	bool log_sample = (xic_sample_client > 0 && (xic_sample_client == 1
			 || (random() / (RAND_MAX + 1.0) * xic_sample_client) < 1));
	bool log_except = xic_except_client && status;

	if (log_answer || log_slow || log_sample || log_except)
	{
		int64_t used_ms = 0;
		const char *tag = status ? "XIC.CAE" : "XIC.CAN";
		int64_t txid = answer->txid();
		const xstr_t *service, *method;
		const xstr_t *q_ctx, *q_args;
		char locus_sync[8];
		if (res)
		{
			Quest* q = res->quest().get();
			service = &q->service();
			method = &q->method();
			q_ctx = &q->context_xstr();
			q_args = &q->args_xstr();
			if (!used_tsc)
				used_tsc = rdtsc() - res->start_tsc();
			used_ms = used_tsc * 1000 / cpu_frequency();
			locus_sync[0] = '/';
			locus_sync[1] = res->isAsync() ? 'A' : 'S';
			locus_sync[2] = '/';
			locus_sync[3] = 0;
		}
		else
		{
			static const xstr_t na = XSTR_CONST("-");
			tag = "XIC.CAX";
			service = &na;
			method = &na;
			q_ctx = &vbs_packed_empty_dict;
			q_args = &vbs_packed_empty_dict;
			locus_sync[0] = '/';
			locus_sync[1] = 0;
		}

		if (log_answer)
		{
			if (status && !xic_dlog_cq && res)
			{
				xdlog(vbs_xfmt, NULL, "XIC.CQE", locus_sync,
					"%u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_RAW<} %p{>VBS_RAW<}",
					_sock_port, _peer_ip, _peer_port,
					(intmax_t)txid, XSTR_P(service), XSTR_P(method), 
					q_ctx, q_args);
			}

			xdlog(vbs_xfmt, NULL, tag, locus_sync,
				"%u/%s+%u %jd Q=%.*s::%.*s T=%d.%03d A=%d %p{>VBS_DICT<}",
				_sock_port, _peer_ip, _peer_port,
				(intmax_t)txid, XSTR_P(service), XSTR_P(method),
				(int)(used_ms / 1000), (int)(used_ms % 1000),
				status, answer->args_dict());
		}

		if (log_slow || log_sample || log_except)
		{
			char locus[64];
			char *p = locus;
			if (log_slow)
				p = stpcpy(p, locus_cli_slow);
			if (log_sample)
				p = stpcpy(p, locus_cli_sample);
			if (log_except)
				p = stpcpy(p, "/EXCEPT");
			p = stpcpy(p, locus_sync);

			xdlog(vbs_xfmt, NULL, "XIC.CQA", locus,
				"T=%d.%03d %u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_RAW<} %p{>VBS_RAW<} A=%d %p{>VBS_DICT<}",
				(int)(used_ms / 1000), (int)(used_ms % 1000),
				_sock_port, _peer_ip, _peer_port,
				(intmax_t)txid, XSTR_P(service), XSTR_P(method), 
				q_ctx, q_args,
				status, answer->args_dict());
		}
	}

	if (res)
	{
		res->giveAnswer(answer);
	}
	else
	{
		std::ostringstream os;
		iobuf_t ob = make_iobuf(os, NULL, 0);
		vbs_print_dict(answer->args_dict(), &ob);

		throw XERROR_FMT(ProtocolException,
			"Unknown answer for msg txid(%lld), local=%s+%u, remote=%s+%u, status=%d, args=%s",
			(long long)answer->txid(),
			_sock_ip, _sock_port, _peer_ip, _peer_port,
			answer->status(), os.str().c_str());
	}
}

void ConnectionI::handle_check(const CheckPtr& check)
{
	const xstr_t& cmd = check->command();
	const VDict& args = check->args();

	if (!_incoming)	// client side
	{
		if (xstr_equal_cstr(&cmd, "FORBIDDEN"))
		{
			xstr_t reason = args.getXstr("reason");
			throw XERROR_MSG(AuthenticationException, make_string(reason));
		}
		else if (xstr_equal_cstr(&cmd, "AUTHENTICATE"))
		{
			if (_ck_state != CK_INIT)
				throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&cmd));

			xstr_t method = args.wantXstr("method");
			if (!xstr_equal_cstr(&method, "SRP6a"))
				throw XERROR_FMT(XError, "Unknown authenticate method [%.*s]", XSTR_P(&method));

			SecretBoxPtr sb = getSecretBox();
			if (!sb)
				throw XERROR_FMT(XError, "No SecretBox supplied");

			xstr_t identity, password;
			std::string host = !_host.empty() ? _host : _peer_ip;
			if (!sb->find(_service, _proto, host, _peer_port, identity, password))
			{
				throw XERROR_FMT(XError, "No matched secret for `%s@%s+%s+%d`",
					_service.c_str(), _proto.c_str(), host.c_str(), _peer_port);
			}

			Srp6aClientPtr srp6aClient = new Srp6aClient();
			_srp6a = srp6aClient;
			srp6aClient->set_identity(identity, password);

			CheckWriter cw("SRP6a1");
			cw.param("I", identity);
			send_kmsg(cw.take());
			_ck_state = CK_S2;
		}
		else if (xstr_equal_cstr(&cmd, "SRP6a2"))
		{
			if (_ck_state != CK_S2)
				throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&cmd));

			xstr_t hash = args.getXstr("hash");
			xstr_t N = args.wantBlob("N");
			xstr_t g = args.wantBlob("g");
			xstr_t s = args.wantBlob("s");
			xstr_t B = args.wantBlob("B");

			Srp6aClientPtr srp6aClient = Srp6aClientPtr::cast(_srp6a);
			srp6aClient->set_hash(hash);
			srp6aClient->set_parameters(g, N, N.len * 8);
			srp6aClient->set_salt(s);
			srp6aClient->set_B(B);
			xstr_t A = srp6aClient->gen_A();
			xstr_t M1 = srp6aClient->compute_M1();

			CheckWriter cw("SRP6a3");
			cw.paramBlob("A", A);
			cw.paramBlob("M1", M1);
			send_kmsg(cw.take());
			_ck_state = CK_S4;
		}
		else if (xstr_equal_cstr(&cmd, "SRP6a4"))
		{
			if (_ck_state != CK_S4)
				throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&cmd));

			xstr_t M2 = args.wantBlob("M2");
			Srp6aClientPtr srp6aClient = Srp6aClientPtr::cast(_srp6a);
			xstr_t M2_mine = srp6aClient->compute_M2();
			if (!xstr_equal(&M2, &M2_mine))
				throw XERROR_FMT(XError, "srp6a M2 not equal");

			_ck_state = CK_FINISH;
			_srp6a.reset();
		}
	}

	// server side
	try
	{
		if (xstr_equal_cstr(&cmd, "SRP6a1"))
		{
			if (_ck_state != CK_S1)
				throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&cmd));

			xstr_t identity = args.wantXstr("I");
			xstr_t method, paramId, salt, verifier;
			if (!_shadowBox->getVerifier(identity, method, paramId, salt, verifier))
				throw XERROR_FMT(XError, "No such identity [%.*s]", XSTR_P(&identity));

			
			Srp6aServerPtr srp6aServer = _shadowBox->newSrp6aServer(paramId);
			ENFORCE(srp6aServer);
			_srp6a = srp6aServer;

			srp6aServer->set_v(verifier);
			xstr_t B = srp6aServer->gen_B();

			CheckWriter cw("SRP6a2");
			cw.param("hash", srp6aServer->hash_name());
			cw.paramBlob("s", salt);
			cw.paramBlob("B", B);
			cw.paramBlob("g", srp6aServer->get_g());
			cw.paramBlob("N", srp6aServer->get_N());
			send_kmsg(cw.take());
			_ck_state = CK_S3;
		}
		else if (xstr_equal_cstr(&cmd, "SRP6a3"))
		{
			if (_ck_state != CK_S3)
				throw XERROR_FMT(XError, "Unexpected command of Check message [%.*s]", XSTR_P(&cmd));

			xstr_t A = args.wantBlob("A");
			xstr_t M1 = args.wantBlob("M1");

			Srp6aServerPtr srp6aServer = Srp6aServerPtr::cast(_srp6a);
			srp6aServer->set_A(A);
			xstr_t M1_mine = srp6aServer->compute_M1();
			if (!xstr_equal(&M1, &M1_mine))
				throw XERROR_FMT(XError, "srp6a M1 not equal");

			xstr_t M2 = srp6aServer->compute_M2();
			CheckWriter cw("SRP6a4");
			cw.paramBlob("M2", M2);
			send_kmsg(cw.take());
			send_kmsg(HelloMessage::create());

			_ck_state = CK_FINISH;
			_state = ST_ACTIVE;
			_shadowBox.reset();
			_srp6a.reset();
			checkFinished();
		}
	}
	catch (XError& ex)
	{
		if (xic_dlog_warning)
			dlog("XIC.WARNING", "peer=%s+%d #=client authentication failed, %s", _peer_ip, _peer_port, ex.message().c_str());

		CheckWriter cw("FORBIDDEN");
		cw.param("reason", ex.message());
		send_kmsg(cw.take());
		throw;
	}
}

WaiterI::WaiterI(const CurrentI& r)
	: _quest(r._quest), _txid(r._txid), _service(r._service), _method(r._method)
{
	_con.reset(static_cast<ConnectionI*>(r.con.get()));
}

AnswerPtr WaiterI::trace(const AnswerPtr& answer) const
{
	return answer->status() ? append_raiser(answer, make_raiser(_method, _service, _con->endpoint()))
				: answer;
}

void WaiterI::response(const std::exception& ex)
{
	response(except2answer(ex, _method, _service, _con->endpoint()), false);
	if (xic_dlog_warning && dynamic_cast<const xic::XicException*>(&ex))
	{
		dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s exception=%s",
			_con->peer_ip(), _con->peer_port(), XSTR_P(&_service), XSTR_P(&_method), ex.what());
	}
}


WaiterImp::WaiterImp(const CurrentI& r)
	: WaiterI(r), _start_tsc(r._start_tsc), _waiting(1), _logit(r._logit)
{
}

WaiterImp::~WaiterImp()
{
	if (xatomic_get(&_waiting) > 0)
	{
		XERROR_VAR_MSG(ServantException, ex, "No answer returned from servant");
		WaiterI::response(ex);
	}
}

bool WaiterImp::responded() const
{
	return (xatomic_get(&_waiting) <= 0);
}

void WaiterImp::response(const AnswerPtr& answer, bool trace)
{
	if (xatomic_dec_and_test(&_waiting))
	{
		AnswerPtr answer2;
		ConnectionI* con = _con.get();
		Answer *a = answer.get();

		if (!a)
		{
			XERROR_VAR_MSG(ServantException, ex, "Null answer responded from servant");
			if (xic_dlog_warning)
			{
				dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s exception=%s",
					con->peer_ip(), con->peer_port(), XSTR_P(&_service), XSTR_P(&_method), ex.what());
			}
			answer2 = except2answer(ex, _method, _service, _con->endpoint());
			a = answer2.get();
		}
		else if (a->status() && trace)
		{
			answer2 = this->trace(answer);
			a = answer2.get();
		}

		int status = a->status();
		int64_t used_tsc = 0;
		bool log_answer = (xic_dlog_sa || (status && xic_dlog_sae));
		bool log_slow = (slow_srv_tsc >= 0 && (used_tsc = rdtsc() - _start_tsc) > slow_srv_tsc);
		bool log_sample = (xic_sample_server > 0 && (xic_sample_server == 1 
				|| (random() / (RAND_MAX + 1.0) * xic_sample_server) < 1));
		bool log_except = xic_except_server && status;
		bool log_mark = _logit;

		if (log_answer || log_slow || log_sample || log_except || log_mark)
		{
			if (!used_tsc)
				used_tsc = rdtsc() - _start_tsc;
			int64_t used_ms = used_tsc * 1000 / cpu_frequency();
			Quest* q = _quest.get();

			if (log_answer)
			{
				if (status && !xic_dlog_sq)
				{
					xdlog(vbs_xfmt, NULL, "XIC.SQE", "/A/",
						"%u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_DICT<} %p{>VBS_DICT<}",
						con->_sock_port, con->_peer_ip, con->_peer_port,
						(intmax_t)_txid, XSTR_P(&_service), XSTR_P(&_method), 
						q->context_dict(), q->args_dict());
				}

				const char *tag = status ? "XIC.SAE" : "XIC.SAN";
				xdlog(vbs_xfmt, NULL, tag, "/A/",
					"%u/%s+%u %jd Q=%.*s::%.*s T=%d.%03d A=%d %p{>VBS_RAW<}",
					con->_sock_port, con->_peer_ip, con->_peer_port,
					(intmax_t)_txid, XSTR_P(&_service), XSTR_P(&_method),
					(int)(used_ms / 1000), (int)(used_ms % 1000),
					status, &a->args_xstr());
			}

			if (log_slow || log_sample || log_except || log_mark)
			{
				char locus[64];
				char *p = locus;
				if (log_slow)
					p = stpcpy(p, locus_srv_slow);
				if (log_sample)
					p = stpcpy(p, locus_srv_sample);
				if (log_except)
					p = stpcpy(p, "/EXCEPT");
				if (log_mark)
					p = stpcpy(p, "/MARK");
				*p++ = '/';
				*p++ = 'A';
				*p++ = '/';
				*p = 0;

				xdlog(vbs_xfmt, NULL, "XIC.SQA", locus,
					"T=%d.%03d %u/%s+%u %jd Q=%.*s::%.*s C%p{>VBS_DICT<} %p{>VBS_DICT<} A=%d %p{>VBS_RAW<}",
					(int)(used_ms / 1000), (int)(used_ms % 1000),
					con->_sock_port, con->_peer_ip, con->_peer_port,
					(intmax_t)_txid, XSTR_P(&_service), XSTR_P(&_method), 
					q->context_dict(), q->args_dict(),
					status, &a->args_xstr());
			}
		}

		a->setTxid(_txid);

		// NB: The check for message size should be after calling answer->setTxid().
		if (a->bodySize() > xic_message_size)
		{
			XERROR_VAR_FMT(ServantException, ex,
				"Huge sized answer responded from servant, size=%u>%u",
				a->bodySize(), xic_message_size);

			if (xic_dlog_warning)
			{
				dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s exception=%s",
					con->peer_ip(), con->peer_port(), XSTR_P(&_service), XSTR_P(&_method), ex.what());
			}
			answer2 = except2answer(ex, _method, _service, _con->endpoint());
			a = answer2.get();
			a->setTxid(_txid);
		}

		con->replyAnswer(answer2.get() ? answer2 : answer);
	}
	else if (xic_dlog_warning)
	{
		dlog("XIC.WARNING", "peer=%s+%d Q=%.*s::%.*s #=Answer already sent",
			_con->peer_ip(), _con->peer_port(), XSTR_P(&_service), XSTR_P(&_method));
	}
}

WaiterPtr CurrentI::asynchronous() const
{
	if (!_waiter)
	{
		if (_txid == 0)
			throw XERROR_MSG(XError, "Waiter for oneway quest");

		_waiter.reset(new WaiterImp(*this));
	}
	return _waiter;
}

AnswerPtr CurrentI::trace(const AnswerPtr& answer) const
{
	return answer->status() ? append_raiser(answer, make_raiser(_method, _service, this->con->endpoint()))
				: answer;
}


AnswerPtr xic::process_servant_method(Servant* srv, const MethodTab* mtab,
		const QuestPtr& quest, const xic::Current& current)
{
	if (!mtab)
		throw XERROR_MSG(XLogicError, "mtab is NULL");

	AnswerPtr answer;
	const xstr_t& method = quest->method();
	const MethodTab::NodeType *node = mtab->find(method);
	if (node)
	{
		xatomic64_inc(&node->ncall);
		if (node->mark)
			current.logIt(true);
		answer = (srv->*node->func)(quest, current);
	}
	else
	{
		static const xstr_t x00ping = XSTR_CONST("\x00ping");
		static const xstr_t x00stat = XSTR_CONST("\x00stat");
		static const xstr_t x00mark = XSTR_CONST("\x00mark");
		static const xstr_t x00 = XSTR_CONST("\x00");

		if (xstr_equal(&method, &x00ping))
		{
			AnswerWriter aw;
			answer = aw.take();
		}
		else if (xstr_equal(&method, &x00stat))
		{
			AnswerWriter aw;
			VDictWriter dw = aw.paramVDict("counter", 0);

			int64_t notFound = xatomic64_get(&mtab->notFound);
			dw.kv("__METHOD_NOT_FOUND__", notFound);

			const MethodTab::NodeType *node = NULL;
			while ((node = mtab->next(node)) != NULL)
			{
				int64_t ncall = xatomic64_get(&node->ncall);
				dw.kv(node->name, ncall);
			}

			VListWriter lw = aw.paramVList("marks", 0);
			while ((node = mtab->next(node)) != NULL)
			{
				if (node->mark)
					lw.v(node->name);
			}

			answer = aw.take();
		}
		else if (xstr_equal(&method, &x00mark))
		{
			VDict args = quest->args();
			xstr_t m = args.wantXstr("method");
			bool on = args.wantBool("on");
			mtab->mark(m, on);

			AnswerWriter aw;
			VListWriter lw = aw.paramVList("marks", 0);
			while ((node = mtab->next(node)) != NULL)
			{
				if (node->mark)
					lw.v(node->name);
			}

			answer = aw.take();
		}
		else
		{
			if (!xstr_start_with(&method, &x00))
				xatomic64_inc(&mtab->notFound);
			throw XERROR_MSG(MethodNotFoundException, make_string(method));
		}
	}
	return answer;
}


MethodTab::PairType EngineI::_methodpairs[] =
{
#define CMD(X)  { #X, XIC_METHOD_CAST(EngineI, X) },
	XIC_ENGINE_ADMIN_SERVANT_CMDS
#undef CMD                                                                                                                 
};

MethodTab EngineI::_methodtab(_methodpairs, XS_ARRCOUNT(_methodpairs));


EngineI::EngineI(const SettingPtr& setting, const std::string& name)
	: ServantI(&_methodtab), _setting(setting), _name(name),
	_stopped(false), _allowSuicide(true)
{
	uuid_t uuid;
	char buf[UUID_STRING_LEN + 1];
	uuid_generate(uuid);
	uuid_string(uuid, buf, sizeof(buf));
	_uuid.assign(buf, UUID_STRING_LEN);
}

void EngineI::allowSuicide(bool ok)
{
	_allowSuicide = ok;
}

XIC_METHOD(EngineI, uuid)
{
	AnswerWriter aw;
	aw.param("uuid", _uuid);
	return aw;
}

XIC_METHOD(EngineI, suicide)
{
	Quest* q = quest.get();
	VDict args = q->args();
	const xstr_t& uuid = args.getXstr("uuid");
	int doom = args.getInt("doom", 0);

	xdlog(vbs_xfmt, NULL, "XIC.SUICIDE", NULL,
		"con=%s txid=%jd Q=%.*s::%.*s C%p{>VBS_DICT<} %p{>VBS_DICT<}",
		current.con->info().c_str(), (intmax_t)q->txid(),
		XSTR_P(&q->service()), XSTR_P(&q->method()), 
		q->context_dict(), q->args_dict());

	if (!_allowSuicide)
	{
		throw XERROR_MSG(xic::XicException, "suicide disallowd");
	}

	if (uuid.len > 0 && !xstr_case_equal_cstr(&uuid, _uuid.c_str()))
	{
		throw XERROR_MSG(xic::XicException, "uuid not match");
	}

	xic::WaiterPtr waiter = current.asynchronous();
	AnswerWriter aw;
	aw.param("uuid", _uuid);
	waiter->response(aw);

	this->shutdown();

	if (doom)
	{
		if (doom < 0)
		{
			exit(2);
		}
		else
		{
			// wait some seconds in a new thread and exit(1) forcefully.
			doom = XS_CLAMP(doom, SUICIDE_DOOM_MIN, SUICIDE_DOOM_MAX);
			_doom(doom);
		}
	}

	return xic::ASYNC_ANSWER;
}

static void xaw_dlog(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "dlog", 0);
	dw.kv("sq", xic_dlog_sq);
	dw.kv("sa", xic_dlog_sa);
	dw.kv("sae", xic_dlog_sae);
	dw.kv("cq", xic_dlog_cq);
	dw.kv("ca", xic_dlog_ca);
	dw.kv("cae", xic_dlog_cae);
	dw.kv("warning", xic_dlog_warning);
	dw.kv("debug", xic_dlog_debug);
}

static void xaw_allow(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "allow", 0);
	std::stringstream ss;
	xic_allow_ips->dump(ostream_xio.write, (std::ostream*)&ss);
	dw.kv("ips", ss.str());
}

static void xaw_xlog(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "xlog", 0);
	dw.kv("level", xlog_level);
}

static void xaw_timeout(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "timeout", 0);
	dw.kv("connect", xic_timeout_connect);
	dw.kv("close", xic_timeout_close);
	dw.kv("message", xic_timeout_message);
}

static void xaw_acm(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "acm", 0);
	dw.kv("server", xic_acm_server);
	dw.kv("client", xic_acm_client);
}

static void xaw_slow(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "slow", 0);
	dw.kv("server", xic_slow_server);
	dw.kv("client", xic_slow_client);
}

static void xaw_sample(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "sample", 0);
	dw.kv("server", xic_sample_server);
	dw.kv("client", xic_sample_client);
}

static void xaw_except(AnswerWriter& aw, const char *name)
{
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "except", 0);
	dw.kv("server", xic_except_server);
	dw.kv("client", xic_except_client);
}

static void xaw_rlimit(AnswerWriter& aw, const char *name)
{
	struct rlimit rlim;
	int64_t x;
	VDictWriter dw = aw.paramVDict(name && name[0] ? name : "rlimit", 0);
	x = getrlimit(RLIMIT_CORE, &rlim) == 0 ? rlim.rlim_cur : -2;
	dw.kv("core", x);
	x = getrlimit(RLIMIT_NOFILE, &rlim) == 0 ? rlim.rlim_cur : -2;
	dw.kv("nofile", x);
	x = getrlimit(RLIMIT_AS, &rlim) == 0 ? rlim.rlim_cur : -2;
	dw.kv("as", x);
}

static void xaw_all(AnswerWriter& aw)
{
	xaw_dlog(aw, "xic.dlog");
	xaw_allow(aw, "xic.allow");
	xaw_xlog(aw, "xic.xlog");
	xaw_timeout(aw, "xic.timeout");
	xaw_acm(aw, "xic.acm");
	xaw_slow(aw, "xic.slow");
	xaw_sample(aw, "xic.sample");
	xaw_except(aw, "xic.except");
	xaw_rlimit(aw, "xic.rlimit");
}

XIC_METHOD(EngineI, info)
{
	char buf[64];
	AnswerWriter aw;

	aw.param("dlog.identity", dlog_identity());
	aw.param("engine.uuid", _uuid);
	aw.param("engine.name", _name);

	this->_info(aw);

	aw.param("xic.message.size", xic_message_size);

	unix_uid2user(geteuid(), buf, sizeof(buf));
	aw.param("xic.user", buf);

	unix_gid2group(getegid(), buf, sizeof(buf));
	aw.param("xic.group", buf);

	xaw_all(aw);
	return aw;
}

XIC_METHOD(EngineI, tune)
{
	VDict args = quest->args();

	AnswerWriter aw;
	VDict::Node node;
	if (args.getNode("dlog", node))
	{
		VDict d = node.vdictValue();
		VDict::Node it;
		if (d.getNode("sq", it))
			xic_dlog_sq = it.boolValue();
		if (d.getNode("sa", it))
			xic_dlog_sa = it.boolValue();
		if (d.getNode("sae", it))
			xic_dlog_sae = it.boolValue();
		if (d.getNode("cq", it))
			xic_dlog_cq = it.boolValue();
		if (d.getNode("ca", it))
			xic_dlog_ca = it.boolValue();
		if (d.getNode("cae", it))
			xic_dlog_cae = it.boolValue();
		if (d.getNode("warning", it))
			xic_dlog_warning = it.boolValue();
		if (d.getNode("debug", it))
			xic_dlog_debug = it.boolValue();

		xaw_dlog(aw, NULL);
	}

	if (args.getNode("allow", node))
	{
		VDict d = node.vdictValue();
		VDict::Node it;
		if (d.getNode("ips", it))
			xic_allow_ips->reset(it.xstrValue());
		xaw_allow(aw, NULL);
	}

	if (args.getNode("xlog", node))
	{
		VDict d = node.vdictValue();
		int n;

		if ((n = d.getInt("level", INT_MIN)) != INT_MIN)
		{
			xlog_level = n;
		}

		xaw_xlog(aw, NULL);
	}

	if (args.getNode("timeout", node))
	{
		VDict d = node.vdictValue();
		int64_t n;

		if ((n = d.getInt("connect", INT64_MIN)) != INT64_MIN)
		{
			xic_timeout_connect = n > 0 ? n : 0;
		}

		if ((n = d.getInt("close", INT64_MIN)) != INT64_MIN)
		{
			xic_timeout_close = n > 0 ? n : 0;
		}

		if ((n = d.getInt("message", INT64_MIN)) != INT64_MIN)
		{
			xic_timeout_message = n > 0 ? n : 0;
		}

		xaw_timeout(aw, NULL);
	}

	if (args.getNode("acm", node))
	{
		VDict d = node.vdictValue();
		int64_t n;

		if ((n = d.getInt("server", INT64_MIN)) != INT64_MIN)
		{
			xic_acm_server = n > 0 ? n : 0;
		}

		if ((n = d.getInt("client", INT64_MIN)) != INT64_MIN)
		{
			xic_acm_client = n > 0 ? n : 0;
		}

		xaw_acm(aw, NULL);
	}

	if (args.getNode("slow", node))
	{
		VDict d = node.vdictValue();
		int64_t n;

		if ((n = d.getInt("server", INT64_MIN)) != INT64_MIN)
		{
			xic_slow_server = n < 0 ? -1 : n < INT_MAX ? n : INT_MAX;
		}

		if ((n = d.getInt("client", INT64_MIN)) != INT64_MIN)
		{
			xic_slow_client = n < 0 ? -1 : n < INT_MAX ? n : INT_MAX;
		}

		prepare_locus_slow();
		xaw_slow(aw, NULL);
	}

	if (args.getNode("sample", node))
	{
		int64_t x;
		VDict d = node.vdictValue();

		if ((x = d.getInt("server", INT64_MIN)) != INT64_MIN)
		{
			xic_sample_server = x < 0 ? 0 : x < MAX_SAMPLE ? x : MAX_SAMPLE;
		}

		if ((x = d.getInt("client", INT64_MIN)) != INT64_MIN)
		{
			xic_sample_client = x < 0 ? 0 : x < MAX_SAMPLE ? x : MAX_SAMPLE;
		}

		prepare_locus_sample();
		xaw_sample(aw, NULL);
	}

	if (args.getNode("except", node))
	{
		VDict d = node.vdictValue();
		VDict::Node nd;

		if (d.getNode("server", nd))
		{
			xic_except_server = nd.boolValue();
		}

		if (d.getNode("client", nd))
		{
			xic_except_client = nd.boolValue();
		}

		xaw_except(aw, NULL);
	}

	if (args.getNode("rlimit", node))
	{
		VDict d = node.vdictValue();
		struct rlimit rlim;
		int64_t x;

		if ((x = d.getInt("core", INT64_MIN)) != INT64_MIN)
		{
			if (getrlimit(RLIMIT_CORE, &rlim) == 0)
			{
				if (x == -1)
					x = RLIM_INFINITY;
				rlim.rlim_cur = x;
				int rc = setrlimit(RLIMIT_CORE, &rlim);
				if (rc < 0 && xic_dlog_warning)
				{
					dlog("XIC.WARNING", "setrlimit() failed, errno=%d %m", errno);
				}
			}
		}

		if ((x = d.getInt("nofile", INT64_MIN)) != INT64_MIN)
		{
			if (getrlimit(RLIMIT_NOFILE, &rlim) == 0 && rlim.rlim_cur < (uint64_t)x)
			{
				if (x == -1)
					x = RLIM_INFINITY;
				rlim.rlim_cur = x;
				int rc = setrlimit(RLIMIT_NOFILE, &rlim);
				if (rc < 0 && xic_dlog_warning)
				{
					dlog("XIC.WARNING", "setrlimit() failed, errno=%d %m", errno);
				}
			}
			else if (rlim.rlim_cur > (uint64_t)x && xic_dlog_warning)
			{
				dlog("XIC.WARNING", "Not permitted to set rlimit.nofile less than curent value");
			}
		}

		if ((x = d.getInt("as", INT64_MIN)) != INT64_MIN)
		{
			if (getrlimit(RLIMIT_AS, &rlim) == 0 && rlim.rlim_cur < (uint64_t)x)
			{
				if (x == -1)
					x = RLIM_INFINITY;
				rlim.rlim_cur = x;
				int rc = setrlimit(RLIMIT_AS, &rlim);
				if (rc < 0 && xic_dlog_warning)
				{
					dlog("XIC.WARNING", "setrlimit() failed, errno=%d %m", errno);
				}
			}
			else if (rlim.rlim_cur > (uint64_t)x && xic_dlog_warning)
			{
				dlog("XIC.WARNING", "Not permitted to set rlimit.as less than curent value");
			}
		}

		xaw_rlimit(aw, NULL);
	}

	int mask = args.getInt("umask", -1);
	if (mask >= 0)
	{
		mask &= 0777;
		umask(mask);
		aw.param("umask", mask);
	}

	std::string user = make_string(args.getXstr("user"));
	std::string group = make_string(args.getXstr("group"));
	if (!user.empty() || !group.empty())
	{
		if (unix_set_user_group(user.c_str(), group.c_str()) < 0 && xic_dlog_warning)
		{
			dlog("XIC.WARNING", "Failed to set process user to '%s' or group to '%s'", user.c_str(), group.c_str());
		}

		char buf[64];
		if (!user.empty())
		{
			unix_uid2user(geteuid(), buf, sizeof(buf));
			aw.param("user", buf);
		}

		if (!group.empty())
		{
			unix_gid2group(getegid(), buf, sizeof(buf));
			aw.param("group", buf);
		}
	}

	return aw;
}

static void _help(const char *program)
{
	fprintf(stderr, "\nUsage: %s --xic.conf=<config_file> [--AAA.BBB=ZZZ]\n\n",
		program);
	exit(1);
}

int ApplicationI::main(int argc, char **argv, const SettingPtr& setting)
{
	int ret = EXIT_FAILURE;
	int app_argc = 0;
	char **app_argv = (char **)malloc(argc * sizeof(char *));
	char *prog_name = strdup(argv[0]);
	char *identity = basename(prog_name);

	dlog_set(identity, 0);
	xlog_set_writer(_xlog_writer);

	try
	{
		char *configfile = NULL;
		int clset = 0;
		app_argv[app_argc++] = argv[0];
		for (int i = 1; i < argc; ++i)
		{
			char *s = argv[i];
			if (strcmp(s, "--help") == 0 || strcmp(s, "-?") == 0)
			{
				_help(identity);
			}

			if (s[0] == '-' && s[1] == '-' && strchr(s + 2, '.'))
			{
				// --xic.conf=configfile
				if (strncmp(s + 2, "xic.", 4) == 0 && strncmp(s + 6, "conf=", 5) == 0)
				{
					configfile = &s[11];
				}
				else
				{
					// --AAA.BBB=ZZZ
					// --AAA.BBB.CCC=ZZZ
					++clset;
					app_argv[argc - clset] = &s[2];
				}
			}
			else
			{
				app_argv[app_argc++] = argv[i];
			}
		}

		_setting = setting ? setting : newSetting();
		if (configfile)
			_setting->load(configfile);

		for (int i = 1; i <= clset; ++i)
		{
			char *s = app_argv[argc - i];
			char *p = strchr(s, '=');
			if (p)
			{
				_setting->set(std::string(s, p - s), std::string(p + 1));
			}
		}

		const char *id = dlog_identity();
		_setting->insert("xic.dlog.identity", id[0] ? id : identity);
		dlog_set(_setting->getString("xic.dlog.identity").c_str(), 0);

		_engine = _engine_creator(_setting, "");

		ret = this->run(app_argc, app_argv);
		_engine->shutdown();
	}
	catch (std::exception& ex)
	{
		XError* x = dynamic_cast<XError*>(&ex);
		xlog(XLOG_EMERG, "EXCEPTION: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
		_help(identity);
	}

	free(prog_name);
	free(app_argv);
	return ret;
}

