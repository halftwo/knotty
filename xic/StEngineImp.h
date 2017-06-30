#ifndef StEngineImp_h_
#define StEngineImp_h_

#include "EngineImp.h"
#include "STimer.h"
#include "xslib/xstr.h"
#include "xslib/obpool.h"
#include <st.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <list>
#include <iostream>


namespace xic
{

class StResult;
typedef XPtr<StResult> StResultPtr;

class StConnection;
typedef XPtr<StConnection> StConnectionPtr;

class StListener;
typedef XPtr<StListener> StListenerPtr;

class StAdapter;
typedef XPtr<StAdapter> StAdapterPtr;

class StProxy;
typedef XPtr<StProxy> StProxyPtr;

class StEngine;
typedef XPtr<StEngine> StEnginePtr;


class StResult: public ResultI
{
	st_cond_t _cond;

	static obpool_t _pool;

	void _notify();
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

	StResult(ProxyI* prx, const QuestPtr& quest, const CompletionPtr& cp);
	virtual ~StResult();

	static void *operator new(size_t size) 		{ return obpool_acquire(&_pool); }
	static void operator delete(void *p) 		{ return obpool_release(&_pool, p); }
};


class StConnection: public ConnectionI, public STimerTask
{
	StEnginePtr _engine;

	st_netfd_t _sf;

	STimerTaskPtr _writeTimeoutTask;

	st_thread_t _recv_sth;
	st_thread_t _send_sth;
	st_cond_t _recv_cond;
	st_cond_t _send_cond;

	bool _recent_active;
	time_t _active_time;
	void init();
	void set_exception(XError *ex);

public:
	virtual void close(bool force);
	virtual ProxyPtr createProxy(const std::string& service);
	virtual void setAdapter(const AdapterPtr& adapter) 	{ _adapter = adapter; }
	virtual AdapterPtr getAdapter() const 			{ return _adapter; }

public:
	virtual void sendQuest(const QuestPtr& quest, const ResultIPtr& r);
	virtual void replyAnswer(const AnswerPtr& answer);
	virtual int disconnect();
	virtual void send_kmsg(const XicMessagePtr& msg);
	virtual SecretBoxPtr getSecretBox();
	virtual void checkFinished();

public:
	StConnection(StEngine* engine, StAdapter* adapter, st_netfd_t sf, int timeout, int close_timeout);
	StConnection(StEngine* engine, const std::string& service, const std::string& endpoint, int attempt);
	virtual ~StConnection();

	void recv_fiber();
	void send_fiber();
	int on_write_timeout();
	bool reap_idle(time_t now, time_t before);

private:
	virtual void runTimerTask(const STimerPtr& timer)
	{
		do_timeout(0);
	}

	void _grace();
	void _send_qmsg(const XicMessagePtr& msg);
	void do_timeout(int rw);
	int recv_msg(XicMessagePtr& msg); 
};


class StListener: virtual public XRefCount
{
	StEnginePtr _engine;
	StAdapterPtr _adapter;
	int _msg_timeout;
	int _close_timeout;
	st_netfd_t _sf;
	st_thread_t _accept_thr;
public:
	StListener(StEngine* engine, StAdapter* adapter, int fd, int timeout, int close_timeout);
	virtual ~StListener();

	void activate();
	void deactivate();

public:
	void accept_fiber();
};

class StAdapter: public Adapter
{
	typedef std::map<std::string, ServantPtr> ServantMap;

	StEnginePtr _engine;
	std::string _name;
	std::string _endpoints;
	std::vector<StListenerPtr> _listeners;
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
	StAdapter(StEngine* engine, const std::string& name, const std::string& endpoints);
	virtual ~StAdapter();

	virtual EnginePtr getEngine() const 			{ return _engine; }
	virtual const std::string& name() const 		{ return _name; }
	virtual const std::string& endpoints() const 		{ return _endpoints; }

	virtual void activate();
	virtual void deactivate();

	virtual ProxyPtr addServant(const std::string& service, Servant* servant);
	virtual ServantPtr removeServant(const std::string& service);
	virtual ServantPtr findServant(const std::string& service) const;

	virtual ServantPtr getDefaultServant() const		{ return _default; }
	virtual void setDefaultServant(Servant* servant) 	{ _default.reset(servant); }
	virtual void unsetDefaultServant()			{ _default.reset(); }

public:
	std::vector<std::string> getServices() const;

	void wait();
};


class StProxy: public ProxyI
{
public:
	virtual ContextPtr getContext() const;
	virtual void setContext(const ContextPtr& ctx);
	virtual ConnectionPtr getConnection() const;
	virtual void resetConnection();
	virtual ResultPtr emitQuest(const QuestPtr& quest, const CompletionPtr& cp);
	virtual bool retryQuest(const ResultIPtr& r);

public:
	StProxy(const std::string& service, EngineI* engine, StConnection* con)
		: ProxyI(service, engine, con)
	{
	}

	StProxy(const std::string& proxy, EngineI* engine)
		: ProxyI(proxy, engine)
	{
	}

	virtual void onConnectionError(const ConnectionIPtr& con, const QuestPtr& quest);
};


struct StThreadPool: public XRefCount
{
	size_t _thrNum;
	size_t _thrMax;
	size_t _stackSize;
	st_cond_t _thrCond;

	StThreadPool();
	~StThreadPool();

	void wait_thread();
	void signal_thread();
};
typedef XPtr<StThreadPool> StThreadPoolPtr;


class StEngine: public EngineI, public STimerTask
{
	struct StThrob;
	typedef std::map<std::string, StAdapterPtr> AdapterMap;
	typedef std::map<std::string, StProxyPtr> ProxyMap;
	typedef std::map<std::string, StConnectionPtr> ConnectionMap;
	typedef std::list<StConnectionPtr> ConnectionList;

	AdapterMap _adapterMap;
	ProxyMap _proxyMap;
	ConnectionMap _conMap;
	ConnectionList _incomingCons;
	st_cond_t _runCond;
	STimerPtr _timer;
	StThreadPoolPtr _srvPool;
	StThreadPoolPtr _cliPool;
	XPtr<StThrob> _throb;

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
	virtual ConnectionIPtr makeConnection(const std::string& service, const std::string& endpoint);

public:
	StEngine(const SettingPtr& setting, const std::string& name);
	virtual ~StEngine();

	void incomingConnection(const StConnectionPtr& con);
	const STimerPtr& getTimer() const 			{ return _timer; }
	size_t serverStackSize() const				{ return _srvPool->_stackSize; }
	size_t clientStackSize() const				{ return _cliPool->_stackSize; }

	int check_stop();

	template<class T>
	st_thread_t create_server_thread(T* obj, void (T::*mf)());

	template<class T>
	st_thread_t create_client_thread(T* obj, void (T::*mf)());

private:
	friend class StConnection;
	ProxyPtr _makeFixedProxy(const std::string& service, StConnection* con);
	virtual void runTimerTask(const STimerPtr& timer);
	virtual void _doom(int seconds);
	virtual void _info(AnswerWriter& aw);
};


};

#endif
