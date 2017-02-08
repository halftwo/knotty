#ifndef PtEngineImp_h_
#define PtEngineImp_h_

#include "EngineImp.h"
#include "xslib/xstr.h"
#include "xslib/loc.h"
#include "xslib/obpool.h"
#include "xslib/iobuf.h"
#include "xslib/XEvent.h"
#include "xslib/XTimer.h"
#include "xslib/XLock.h"
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <list>
#include <iostream>
#include <memory>

namespace xic
{

class PtResult;
typedef XPtr<PtResult> PtResultPtr;

class PtConnection;
typedef XPtr<PtConnection> PtConnectionPtr;

class PtListener;
typedef XPtr<PtListener> PtListenerPtr;

class PtAdapter;
typedef XPtr<PtAdapter> PtAdapterPtr;

class PtProxy;
typedef XPtr<PtProxy> PtProxyPtr;

class PtEngine;
typedef XPtr<PtEngine> PtEnginePtr;


class PtResult: public ResultI
{
	enum { COND_MASK = 1023 };
	static XMutex _mutex;
	static XCond _conds[COND_MASK + 1];

public:	
	virtual bool isSent() const;
	virtual void waitForSent();
	virtual bool isCompleted() const;
	virtual void waitForCompleted();
	virtual AnswerPtr takeAnswer(bool throw_ex);

public:
	virtual bool retry();
	virtual void questSent();
	virtual void giveAnswer(AnswerPtr& answer);
	virtual void giveError(const XError& ex);

	PtResult(ProxyI* prx, const QuestPtr& quest, const CompletionPtr& cp)
		: ResultI(prx, quest, cp)
	{
	}
};


class PtConnection: public ConnectionI, public XEvent::FdHandler, public XTimerTask, private XRecMutex
{
	PtEnginePtr _engine;
	int _fd;

	XTimerTaskPtr _writeTimeoutTask;

	loc_t _iloc;
	int _ipos;
	XicMessagePtr _rMsg;
	iobuf_t _ib;
	unsigned char _ibuf[1024];

	loc_t _oloc;
	struct iovec *_ov;
	int _ov_num;
	bool _recent_active;
	time_t _active_time;

	XicMessagePtr _wMsg;

	void init();
	void set_exception(XError *ex);

public:
	virtual void close(bool force);
	virtual ProxyPtr createProxy(const std::string& service);
	virtual void setAdapter(const AdapterPtr& adapter);
	virtual AdapterPtr getAdapter() const;
	virtual State state() const		{ Lock lock(*this); return _state; }

public:
	virtual void sendQuest(const QuestPtr& quest, const ResultIPtr& r);
	virtual void replyAnswer(const AnswerPtr& answer);
	virtual int disconnect();
	virtual void send_kmsg(const XicMessagePtr& kmsg);
	virtual SecretBoxPtr getSecretBox();
	virtual void checkFinished();

public:
	PtConnection(PtEngine* engine, PtAdapter* adapter, int fd, int timeout, int close_timeout);
	PtConnection(PtEngine* engine, const std::string& service, const std::string& endpoint, int attempt);
	virtual ~PtConnection();

	void start();
	int on_write_timeout();
	bool reap_idle(time_t now, time_t before);

private:
	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);

	virtual void runTimerTask(const XTimerPtr& timer)
	{
		do_timeout(0);
	}

	void _grace(Lock& lock);
	void _send_qmsg(Lock& lock, const XicMessagePtr& qmsg);
	int _write_q(Lock& lock);
	int do_read(const XEvent::DispatcherPtr& dispatcher);
	int do_write();
	void do_timeout(int rw);
	int recv_msg(XicMessagePtr& msg);
};


class PtListener: public XEvent::FdHandler
{
	PtEnginePtr _engine;
	PtAdapterPtr _adapter;
	int _fd;
	int _msg_timeout;
	int _close_timeout;
public:
	PtListener(PtEngine* engine, PtAdapter* adapter, int fd, int timeout, int close_timeout);
	virtual ~PtListener();

	void activate();
	void deactivate();

private:
	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events);
};


class PtAdapter: public Adapter, private XMutex
{
	typedef std::map<std::string, ServantPtr> ServantMap;

	PtEnginePtr _engine;
	std::string _name;
	std::string _endpoints;
	std::vector<PtListenerPtr> _listeners;
	ServantMap _srvMap;
	mutable ServantMap::const_iterator _hint;
	ServantPtr _default;
	enum
	{
		ADAPTER_INIT,
		ADAPTER_ACTIVE,
		ADAPTER_FINISHED,
	} _state;

	int _createListener(xic::EndpointInfo& ei, const char *ip);
	void _appendEndpoint(xic::EndpointInfo& ei, const char *ip, int port);

public:
	PtAdapter(PtEngine* engine, const std::string& name, const std::string& endpoints);
	virtual ~PtAdapter();

	virtual EnginePtr getEngine() const 			{ return _engine; }
	virtual const std::string& name() const 		{ return _name; }
	virtual const std::string& endpoints() const 		{ return _endpoints; }

	virtual void activate();
	virtual void deactivate();

	virtual ProxyPtr addServant(const std::string& service, Servant* servant);
	virtual ServantPtr removeServant(const std::string& service);
	virtual ServantPtr findServant(const std::string& service) const;

	virtual ServantPtr getDefaultServant() const;
	virtual void setDefaultServant(Servant* servant);
	virtual void unsetDefaultServant();

public:
	std::vector<std::string> getServices() const;
};


class PtProxy: public ProxyI, private XMutex
{
public:
	virtual ContextPtr getContext() const;
	virtual void setContext(const ContextPtr& ctx);
	virtual ConnectionPtr getConnection() const;
	virtual void resetConnection();
	virtual ResultPtr emitQuest(const QuestPtr& quest, const CompletionPtr& cp);
	virtual bool retryQuest(const ResultIPtr& r);

public:
	PtProxy(const std::string& service, EngineI* engine, PtConnection* con)
		: ProxyI(service, engine, con)
	{
	}

	PtProxy(const std::string& proxy, EngineI* engine)
		: ProxyI(proxy, engine)
	{
	}

	virtual void onConnectionError(const ConnectionIPtr& con, const QuestPtr& quest);
};


class PtEngine: public EngineI, public XTimerTask, private XMutex
{
	struct PtThrob;
	typedef std::map<std::string, PtAdapterPtr> AdapterMap;
	typedef std::map<std::string, PtProxyPtr> ProxyMap;
	typedef std::map<std::string, PtConnectionPtr> ConnectionMap;
	typedef std::list<PtConnectionPtr> ConnectionList;

	AdapterMap _adapterMap;
	ProxyMap _proxyMap;
	ConnectionMap _conMap;
	ConnectionList _incomingCons;
	XTimerPtr _timer;
	XEvent::DispatcherPtr _srvDispatcher;
	XEvent::DispatcherPtr _cliDispatcher;
	XPtr<PtThrob> _throb;

public:
	virtual AdapterPtr createAdapter(const std::string& name = "", const std::string& endpoints = "");
	virtual AdapterPtr createSlackAdapter();
	virtual ProxyPtr stringToProxy(const std::string& proxy);
	virtual void setSecretBox(const SecretBoxPtr& secretBox);
	virtual void setShadowBox(const ShadowBoxPtr& shadowBox);
	virtual SecretBoxPtr getSecretBox();
	virtual ShadowBoxPtr getShadowBox();
	virtual void throb(const std::string& logword);
	virtual void shutdown();
	virtual void waitForShutdown();
	virtual time_t time();
	virtual int sleep(int seconds);

public:
	ConnectionIPtr makeConnection(const std::string& service, const std::string& endpoint);

public:
	PtEngine(const SettingPtr& setting, const std::string& name);
	virtual ~PtEngine();

	void incomingConnection(const PtConnectionPtr& con);
	const XTimerPtr& getTimer() const 				{ return _timer; }
	const XEvent::DispatcherPtr& getServerDispatcher() const 	{ return _srvDispatcher; }
	const XEvent::DispatcherPtr& getClientDispatcher() const 	{ return _cliDispatcher; }

	int check_stop();
private:
	virtual void runTimerTask(const XTimerPtr& timer);
	virtual void _doom(int seconds);
	virtual void _info(AnswerWriter& aw);
	void _createServerDispatcher();
	void _createClientDispatcher();
	void _serverThreadPoolSetting(size_t& thrSize, size_t& thrMax, size_t& stackSize);
	void _clientThreadPoolSetting(size_t& thrSize, size_t& thrMax, size_t& stackSize);
};


};

#endif
