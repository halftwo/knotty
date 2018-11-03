#ifndef EngineImp_h_
#define EngineImp_h_

#include "Engine.h"
#include "XicMessage.h"
#include "XicCheck.h"
#include "ServantI.h"
#include "IpMatcher.h"
#include "SecretBox.h"
#include "ShadowBox.h"
#include "MyCipher.h"
#include "xslib/rdtsc.h"
#include "xslib/cxxstr.h"
#include "xslib/CarpSequence.h"
#include "xslib/UniquePtr.h"
#include <sstream>
#include <map>
#include <deque>


namespace xic
{

class ResultMap;
class ResultI;
class ConnectionI;
class ProxyI;
class EngineI;
class WaiterI;
struct CurrentI;

typedef XPtr<ResultI> ResultIPtr;
typedef XPtr<ConnectionI> ConnectionIPtr;
typedef XPtr<ProxyI> ProxyIPtr;
typedef XPtr<EngineI> EngineIPtr;
typedef XPtr<WaiterI> WaiterIPtr;

class HelloMessage;
class ByeMessage;
typedef XPtr<HelloMessage> HelloMessagePtr;
typedef XPtr<ByeMessage> ByeMessagePtr;

extern uint32_t xic_message_size;

extern bool xic_dlog_sq;
extern bool xic_dlog_sa;
extern bool xic_dlog_sae;

extern bool xic_dlog_cq;
extern bool xic_dlog_ca;
extern bool xic_dlog_cae;

extern bool xic_dlog_warning;	// true
extern bool xic_dlog_debug;

// in milliseconds, 0 for no timeout
extern int xic_timeout_connect;
extern int xic_timeout_close;
extern int xic_timeout_message;

// in seconds, 0 to disable
extern int xic_acm_server;
extern int xic_acm_client;

extern int xic_sample_server;
extern int xic_sample_client;

// in milliseconds, -1 to disable
extern int xic_slow_server;
extern int xic_slow_client;

extern bool xic_except_server;
extern bool xic_except_client;

extern IpMatcherPtr xic_allow_ips;
extern SecretBoxPtr xic_passport_secret;
extern ShadowBoxPtr xic_passport_shadow;
extern MyCipher::CipherSuite xic_cipher;
extern int xic_cipher_mode;

extern EnginePtr xic_engine;


void adjustTSC(uint64_t frequency);

void prepareEngine(const SettingPtr& setting);

void readyToServe(const SettingPtr& setting);

size_t getIps(const xstr_t& host, uint32_t ipv4s[], int *v4num, uint8_t ipv6s[][16], int *v6num, bool& any);


size_t cli_sample_locus(char *buf);

struct iovec *get_msg_iovec(const XicMessagePtr& msg, int *count, const MyCipherPtr& cipher);
void free_msg_iovec(const XicMessagePtr& msg, struct iovec *iov);


AnswerPtr except2answer(const std::exception& ex, const xstr_t& method, const xstr_t& service,
			const std::string& endpoint, bool local = false);



struct EndpointInfo
{
	xstr_t proto;
	xstr_t host;
	uint16_t port;
	int timeout;
	int close_timeout;
	int connect_timeout;

	EndpointInfo()
		: proto(xstr_null), host(xstr_null), port(0), 
		timeout(0), close_timeout(0), connect_timeout(0)
	{
	}

	std::string str() const
	{
		std::stringstream ss;
		ostream_printf(ss, "%.*s+%.*s+%d", XSTR_P(&proto), XSTR_P(&host), port);
		if (timeout || close_timeout || connect_timeout)
		{
			ostream_printf(ss, " timeout=%d", timeout);
			if (close_timeout || connect_timeout)
				ostream_printf(ss, ",%d,%d", close_timeout, connect_timeout);
		}
		return ss.str();
	}
};

void parseEndpoint(const xstr_t& endpoint, xic::EndpointInfo& ei);


class HelloMessage: public XicMessage
{
	HelloMessage(ostk_t *ostk);
public:
	static HelloMessagePtr create();
	virtual void unpack_body()			{}
	virtual struct iovec* body_iovec(int* count);
};


class ByeMessage: public XicMessage
{
	ByeMessage(ostk_t *ostk);
public:
	static ByeMessagePtr create();
	virtual void unpack_body()			{}
	virtual struct iovec* body_iovec(int* count);
};


class ResultI: public Result
{
private:
	uint64_t _start_tsc;

protected:
	ConnectionIPtr _con;
	ProxyIPtr _prx;
	QuestPtr _quest;
	AnswerPtr _answer;
	UniquePtr<XError> _ex;
	int _retryNumber;
	bool _isSent;
	bool _waitSent;
	bool _waitCompleted;
	CompletionPtr _completion;

public:
	ResultI(ProxyI* prx, const QuestPtr& quest, const CompletionPtr& cp)
		: _start_tsc(rdtsc()), _prx(prx), _quest(quest), _retryNumber(0), 
		_isSent(false), _waitSent(false), _waitCompleted(false), _completion(cp)
	{
	}

	virtual void setConnection(const ConnectionIPtr& con)	{ _con = con; }

	virtual ConnectionPtr getConnection() const 	{ return _con; }
	virtual ProxyPtr getProxy() const 		{ return _prx; }
	virtual const QuestPtr& quest() const 		{ return _quest; }

	virtual bool retry() 				= 0;
	virtual void questSent() 			= 0;
	virtual void giveAnswer(AnswerPtr& answer)	= 0;
	virtual void giveError(const XError& ex) 	= 0;

	uint64_t start_tsc() const 			{ return _start_tsc; }
	bool isAsync() const				{ return bool(_completion); }

};


class ResultMap
{
public:
	ResultMap();

	size_t size() const 				{ return _map.size(); }

	int64_t generateId();
	int64_t addResult(const ResultIPtr& result);
	bool addResult(int64_t txid, const ResultIPtr& result);
	ResultIPtr removeResult(int64_t txid);
	ResultIPtr findResult(int64_t txid) const;

	void take(std::map<int64_t, ResultIPtr>& theMap)
	{
		_map.swap(theMap);
		_map.clear();
	}

private:
	int64_t _next_id;
	std::map<int64_t, ResultIPtr> _map;
};


class ProxyI: public Proxy
{
protected:
	EngineIPtr _engine;
	std::string _proxy;
	std::string _service;
	ContextPtr _ctx;
	std::vector<std::string> _endpoints;
	std::vector<ConnectionIPtr> _cons;
	int _idx;
	LoadBalance _loadBalance;
	bool _incoming;
	UniquePtr<CarpSequence> _cseq;

protected:
	ProxyI(const std::string& proxy, EngineI* engine, ConnectionI* con = 0);
	virtual ~ProxyI();

	ConnectionIPtr pickConnection(const QuestPtr& quest);

public:
	virtual EnginePtr getEngine() const			{ return _engine; }
	virtual std::string str() const 			{ return _proxy; }
	virtual std::string service() const 			{ return _service; }
	virtual LoadBalance loadBalance() const			{ return _loadBalance; };
	virtual ProxyPtr timedProxy(int timeout, int close_timeout = 0, int connect_timeout = 0) const;

public:
	virtual void onConnectionError(const ConnectionIPtr& con, const QuestPtr& quest) = 0;
	virtual bool retryQuest(const ResultIPtr& r) = 0;
};


inline bool isLive(Connection::State st)	{ return st < Connection::ST_CLOSE; }
inline bool isWaiting(Connection::State st)	{ return st < Connection::ST_ACTIVE; }
inline bool isActive(Connection::State st) 	{ return st == Connection::ST_ACTIVE; }
inline bool isGraceful(Connection::State st)	{ return st >= Connection::ST_CLOSE && st < Connection::ST_ERROR; }
inline bool isBad(Connection::State st)		{ return st >= Connection::ST_ERROR; }


class ConnectionI: public Connection
{
protected:
	friend class WaiterImp;
	AdapterPtr _adapter;
	std::string _endpoint;
	bool _graceful;
	bool _incoming;
	int _msg_timeout;
	int _close_timeout;
	int _connect_timeout;

	/* NB: _service is just a hint.
	 * The real service string used to quest a server may differ.
 	 */
	std::string _service;
	std::string _proto;
	std::string _host;
	std::string _info;

	UniquePtr<XError> _ex;
	time_t _ex_time;
	enum State _state;
	enum CheckState
	{
		CK_INIT,
		CK_S1,
		CK_S2,
		CK_S3,
		CK_S4,
		CK_FINISH,
	} _ck_state;

	char _sock_ip[40];
	char _peer_ip[40];
	uint16_t _sock_port;
	uint16_t _peer_port;

	ShadowBoxPtr _shadowBox;
	XPtr<Srp6aBase> _srp6a;
	MyCipherPtr _cipher;

	typedef std::deque<XicMessagePtr> MessageQueue;
	MessageQueue _kq;	// prioritized for check msgs
	MessageQueue _wq;	// for normal quest and answer msgs
	ResultMap _resultMap;
	int _processing;	// Only twoway quest is counted.
	int _attempt;		// Number of connecting attempt to the same endpoint.

protected:
	void handle_quest(const AdapterPtr& adapter, CurrentI& current);
	void handle_answer(AnswerPtr& answer, const ResultIPtr& result);
	void handle_check(const CheckPtr& check);

	virtual void send_kmsg(const XicMessagePtr& msg) 	= 0;
	virtual SecretBoxPtr getSecretBox()			= 0;
	virtual void checkFinished()				= 0;

	int closeTimeout();
	int connectTimeout();

public:
	virtual bool incoming() const				{ return _incoming; }
	virtual int timeout() const				{ return _msg_timeout; }

	virtual const std::string& endpoint() const		{ return _endpoint; }
	virtual const std::string& proto() const		{ return _proto; }
	virtual const std::string& info() const			{ return _info; }
	virtual State state() const				{ return _state; }

public:
	virtual void sendQuest(const QuestPtr& quest, const ResultIPtr& r) = 0;
	virtual void replyAnswer(const AnswerPtr& answer) 	= 0;
	virtual int disconnect()				= 0;

	const char *sock_ip() const				{ return _sock_ip; }
	uint16_t sock_port() const				{ return _sock_port; }
	const char *peer_ip() const				{ return _peer_ip; }
	uint16_t peer_port() const				{ return _peer_port; }

	time_t ex_time() const					{ return _ex_time; }
	int attempt() const					{ return _attempt; }

	ConnectionI(bool incoming, int attempt, int timeout, int close_timeout, int connect_timeout);
};


struct CurrentI: public Current
{
	CurrentI(Connection* c, Quest* q)
		: _quest(q)
	{
		this->con.reset(c);
		_txid = q->txid();
		_service = q->service();
		_method = q->method();
		_start_tsc = rdtsc();
		_logit = false;
	}

	virtual WaiterPtr asynchronous() const;
	virtual AnswerPtr trace(const AnswerPtr& answer) const;
	
	virtual void logIt(bool on) const
	{
		_logit = on;
	}

	QuestPtr _quest;
	int64_t _txid;
	xstr_t _service;
	xstr_t _method;
	uint64_t _start_tsc;
	mutable bool _logit;
	mutable WaiterIPtr _waiter;
};


class WaiterImplementation: public Waiter
{
};


class WaiterI: public WaiterImplementation
{
protected:
	ConnectionIPtr _con;
	QuestPtr _quest;
	int64_t _txid;
	xstr_t _service;
	xstr_t _method;

public:
	WaiterI(const CurrentI& r);

	virtual ConnectionPtr getConnection() const 	{ return _con; }
	virtual const QuestPtr& quest() const 		{ return _quest; }
	virtual AnswerPtr trace(const AnswerPtr& answer) const;
	virtual void response(const std::exception& ex);

	virtual void response(const AnswerPtr& answer, bool trace = true) = 0;
	virtual bool responded() const			= 0;
};


class WaiterImp: public WaiterI
{
	uint64_t _start_tsc;
	xatomic_t _waiting;
	bool _logit;

public:
	WaiterImp(const CurrentI& r);
	virtual ~WaiterImp();

	virtual bool responded() const;
	virtual void response(const AnswerPtr& answer, bool trace);
};



#define XIC_ENGINE_ADMIN_SERVANT_CMDS	\
	CMD(id)			\
	CMD(info)		\
	CMD(tune)		\
	CMD(suicide)		\
	/* END OF CMDS */

class EngineI: public Engine, public ServantI
{
protected:
	std::string _id;
	SettingPtr _setting;
	std::string _name;
	bool _stopped;
	bool _allowSuicide;

	virtual void _doom(int seconds)			= 0;
	virtual void _info(xic::AnswerWriter& aw)	= 0;

public:
	EngineI(const SettingPtr& setting, const std::string& name);

	virtual const SettingPtr& setting() const	{ return _setting; }
	virtual const std::string& name() const		{ return _name; }
	virtual const std::string& id() const		{ return _id; }

	virtual ConnectionIPtr makeConnection(const std::string& service, const std::string& endpoint) = 0;
	virtual void allowSuicide(bool ok);

private:
	static MethodTab::PairType _methodpairs[];
	static MethodTab _methodtab;

#define CMD(X)	XIC_METHOD_DECLARE(X);
	XIC_ENGINE_ADMIN_SERVANT_CMDS
#undef CMD
};


	
class ApplicationI
{
	EnginePtr (*_engine_creator)(const SettingPtr&, const std::string&);
	SettingPtr _setting;
	EnginePtr _engine;

protected:
	ApplicationI(EnginePtr (*engine_creator)(const SettingPtr&, const std::string&))
		: _engine_creator(engine_creator)
	{
	}

	virtual ~ApplicationI()
	{
	}

	const SettingPtr& setting() const	{ return _setting; }

	const EnginePtr& engine() const		{ return _engine; }

	/* Must override this member function */
	virtual int run(int argc, char **argv)		= 0;

public:
	/* This member function should be called in the main thread. */
	virtual int main(int argc, char **argv, const SettingPtr& setting = SettingPtr());
};


};


#endif
