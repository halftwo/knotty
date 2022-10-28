#ifndef Engine_h_
#define Engine_h_

#include "XicMessage.h"
#include "XicException.h"
#include "Context.h"
#include "SecretBox.h"
#include "ShadowBox.h"
#include "xslib/XRefCount.h"
#include "xslib/Setting.h"
#include <time.h>
#include <string>
#include <vector>


/* The edition number should be changed when visible modification made.
 * The revision number should be changed when invisible modification made.
 * The release number should be changed when any modification made.
 * Visible modification means change to the interface, api, log format etc.
 * Invisible modification means performance optimization, minor bug fixes etc.
 * The edition and revision numbers consist of:
 * 	2-digit year
 * 	2-digit month
 * 	2-digit day
 * The release number consist of:
 * 	2-digit hour 	start from 10
 */
#define XIC_V_EDITION 	210102
#define XIC_V_REVISION 	22102813

#define XIC_VERSION	XS_TOSTR(XIC_V_EDITION) "." XS_TOSTR(XIC_V_REVISION)


namespace xic
{

class Result;
typedef XPtr<Result> ResultPtr;

class Completion;
typedef XPtr<Completion> CompletionPtr;

class Waiter;
typedef XPtr<Waiter> WaiterPtr;

class Connection;
typedef XPtr<Connection> ConnectionPtr;

class Adapter;
typedef XPtr<Adapter> AdapterPtr;

class Proxy;
typedef XPtr<Proxy> ProxyPtr;

class Servant;
typedef XPtr<Servant> ServantPtr;

class Engine;
typedef XPtr<Engine> EnginePtr;


extern const CompletionPtr NULL_COMPLETION;
extern const AnswerPtr ONEWAY_ANSWER;
extern const AnswerPtr ASYNC_ANSWER;


class Result: virtual public XRefCount
{
public:	
	virtual ConnectionPtr getConnection() const 	= 0;
	virtual ProxyPtr getProxy() const 		= 0;
	virtual const QuestPtr& quest() const 		= 0;

	virtual bool isSent() const 			= 0;
	virtual void waitForSent() 			= 0;
	virtual bool isCompleted() const 		= 0;
	virtual void waitForCompleted() 		= 0;
	virtual AnswerPtr takeAnswer(bool throw_ex) 	= 0;
};


class Completion: virtual public XRefCount
{
public:
	template<class T>
	static CompletionPtr createFromMemberFunction(T *obj, 
				void (T::*completed)(const ResultPtr& result),
				void (T::*sent)(const ResultPtr& result)=NULL);

	static CompletionPtr createPassThrough(const WaiterPtr& waiter);

	virtual void completed(const ResultPtr& result) = 0;
	virtual void sent(const ResultPtr& result) 	{}
};


class Waiter: virtual public XRefCount
{
	Waiter() {}
	friend class WaiterImplementation;
public:
	virtual ConnectionPtr getConnection() const 	= 0;
	virtual const QuestPtr& quest() const 		= 0;
	virtual AnswerPtr trace(const AnswerPtr& answer) const = 0;
	virtual bool responded() const = 0;
	virtual void response(const AnswerPtr& answer, bool trace = true) = 0;
	virtual void response(const std::exception& ex) = 0;
};


class Connection: virtual public XRefCount
{
public:
	enum State
	{
		ST_INIT,
		ST_WAITING_HELLO,
		ST_ACTIVE,
		ST_CLOSE,
		ST_CLOSING,
		ST_CLOSED,
		ST_ERROR,
	};

	virtual void close(bool force) 			= 0;
	virtual ProxyPtr createProxy(const std::string& service) = 0;
	virtual void setAdapter(const AdapterPtr& adapter) = 0;
	virtual AdapterPtr getAdapter() const 		= 0;

	virtual bool incoming() const 			= 0;
	virtual int timeout() const 			= 0;
	virtual const std::string& endpoint() const 	= 0;
	virtual const std::string& proto() const 	= 0;
	virtual const std::string& info() const 	= 0;
	virtual State state() const			= 0;
};


struct Current
{
	virtual ~Current() 				{}
	virtual WaiterPtr asynchronous() const 		= 0;
	virtual AnswerPtr trace(const AnswerPtr& answer) const = 0;
	virtual void logIt(bool on) const 		= 0;

	ConnectionPtr con;
};


#define XIC_MF(METHOD)				_xic__##METHOD

#define XIC_METHOD_DECLARE(METHOD)		\
	virtual xic::AnswerPtr XIC_MF(METHOD)(const xic::QuestPtr& quest, const xic::Current& current)

#define XIC_METHOD_CAST(SERVANT, METHOD)	\
	((xic::Servant::MethodFunction)&SERVANT::XIC_MF(METHOD))

#define XIC_METHOD(SERVANT, METHOD)		\
	xic::AnswerPtr SERVANT::XIC_MF(METHOD)(const xic::QuestPtr& quest, const xic::Current& current)


class Servant: virtual public XRefCount
{
public:
	typedef AnswerPtr (Servant::*MethodFunction)(const QuestPtr& quest, const Current& current);

	virtual AnswerPtr process(const QuestPtr& quest, const Current& current) = 0;
};


class Adapter: virtual public XRefCount
{
public:
	virtual EnginePtr getEngine() const 		= 0;
	virtual const std::string& name() const 	= 0;
	virtual const std::string& endpoints() const 	= 0;

	virtual void activate() 			= 0;
	virtual void deactivate() 			= 0;

	virtual ProxyPtr addServant(const std::string& service, Servant* servant) = 0;
	virtual ServantPtr removeServant(const std::string& service) = 0;
	virtual ServantPtr findServant(const std::string& service) const = 0;

	virtual ServantPtr getDefaultServant() const 	= 0;
	virtual void setDefaultServant(Servant* servant) = 0;
	virtual void unsetDefaultServant() 		= 0;

	ProxyPtr addServant(const std::string& service, const ServantPtr& servant)
	{
		return addServant(service, servant.get());
	}

	void setDefaultServant(const ServantPtr& servant)
	{
		return setDefaultServant(servant.get());
	}
};


class Proxy: virtual public XRefCount
{
public:
	enum LoadBalance
	{
		LB_NORMAL,
		LB_RANDOM,
		LB_HASH,
	};

	virtual EnginePtr getEngine() const		= 0;
	virtual std::string str() const 		= 0;
	virtual std::string service() const		= 0;

	virtual ContextPtr getContext() const		= 0;
	virtual void setContext(const ContextPtr& ctx)	= 0;

	// Only meaningful when LoadBalance == LB_NORMAL
	virtual ConnectionPtr getConnection() const 	= 0;
	virtual void resetConnection()			= 0;

	virtual LoadBalance loadBalance() const		= 0;

	virtual ProxyPtr timedProxy(int timeout, int close_timeout = 0, int connect_timeout = 0) const = 0;

	virtual ResultPtr emitQuest(const QuestPtr& quest, const CompletionPtr& cp) = 0;

	void requestOneway(const QuestPtr& quest);
	AnswerPtr request(const QuestPtr& quest);
};


class Engine: virtual public XRefCount
{
public:
	static EnginePtr pt(const SettingPtr& setting = SettingPtr(), const std::string& name = "");
	static EnginePtr st(const SettingPtr& setting = SettingPtr(), const std::string& name = "");

	virtual const SettingPtr& setting() const	= 0;
	virtual const std::string& name() const		= 0;
	virtual const std::string& id() const		= 0;

	virtual void setSecretBox(const SecretBoxPtr& secretBox) = 0;
	virtual void setShadowBox(const ShadowBoxPtr& shadowBox) = 0;
	virtual SecretBoxPtr getSecretBox()		= 0;
	virtual ShadowBoxPtr getShadowBox()		= 0;

	virtual AdapterPtr createAdapter(const std::string& name = "", const std::string& endpoints = "") = 0;
	virtual AdapterPtr createSlackAdapter() 	= 0;
	virtual ProxyPtr stringToProxy(const std::string& proxy) = 0;
	virtual void throb(const std::string& logword) 	= 0;
	virtual void shutdown() 			= 0;	// should NOT block caller.
	virtual void waitForShutdown()			= 0;	// DO block caller.
	virtual void allowRemoteKill(bool ok)		= 0;	// RemoteKill is disallowed by default.

	virtual time_t time()				= 0;
	virtual int sleep(int seconds)			= 0;	// current thread sleep, not the engine
};


typedef int (*xic_application_function)(int argc, char **argv, const xic::EnginePtr& engine);

int start_xic_pt(xic_application_function run, int argc, char **argv, const SettingPtr& setting = SettingPtr());
int start_xic_st(xic_application_function run, int argc, char **argv, const SettingPtr& setting = SettingPtr());


template<class T>
class CompletionImpl: public Completion
{
	XPtr<T> _obj;
	void (T::*_completed)(const ResultPtr& result);
	void (T::*_sent)(const ResultPtr& result);

	friend class Completion;

	CompletionImpl(T *obj, 
			void (T::*completed)(const ResultPtr& result),
			void (T::*sent)(const ResultPtr& result))
		: _obj(obj), _completed(completed), _sent(sent)
	{
	}

	virtual void completed(const ResultPtr& result)
	{
		if (_completed)
			(_obj.get()->*_completed)(result);
	}

	virtual void sent(const ResultPtr& result)
	{
		if (_sent)
			(_obj.get()->*_sent)(result);
	}
};

template <class T>
inline CompletionPtr Completion::createFromMemberFunction(T *obj, 
			void (T::*completed)(const ResultPtr& result),
			void (T::*sent)(const ResultPtr& result))
{
	return CompletionPtr(new CompletionImpl<T>(obj, completed, sent));
}


/*
 * As conventions:
 * The class that implements a service should be named with a suffix "Servant".
 * The executable should be named with a suffix "Man".
 * These are just conventions, not requirements.
 */

};


#endif

/* vim: set ts=8 sw=8 noet: 
 */
