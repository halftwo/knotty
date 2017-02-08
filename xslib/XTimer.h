/* $Id: XTimer.h,v 1.7 2014/05/15 09:39:34 gremlin Exp $ */
#ifndef XTimer_h_
#define XTimer_h_

#include "XRefCount.h"
#include "xlog.h"
#include <stdint.h>


class XTimer;
class XTimerTask;
class XTimerTaskMan;

typedef XPtr<XTimer> XTimerPtr;
typedef XPtr<XTimerTask> XTimerTaskPtr;


class XTimerTask: virtual public XRefCount
{
public:
	template<class T>
	static XTimerTaskPtr create(T *obj, int (T::*mfun)());

	template<class T>
	static XTimerTaskPtr create(const XPtr<T>& obj, int (T::*mfun)());

	template <typename F, typename P1>
	static XTimerTaskPtr create(F fun, P1 p1);

	XTimerTask();
	virtual ~XTimerTask();

	int64_t msecRun() const			{ return _msec; }

	virtual void runTimerTask(const XTimerPtr& timer) = 0;

private:
	friend class XTimerTaskMan;
	int _index;
	int64_t _msec;
};


class XTimer: virtual public XRefCount
{
public:
	static XTimerPtr create();

	virtual void start() 						= 0;
	virtual void waitForCancel() 					= 0;
	virtual void cancel() 						= 0;

	virtual void runAllWaitingTasks()				= 0;

	virtual int64_t msecMonotonic() const 				= 0;

	// msec in millseconds
	virtual bool addTask(XTimerTask* task, int msec) 		= 0;
	virtual bool replaceTask(XTimerTask* task, int msec) 		= 0;
	virtual bool replaceTaskLaterThan(XTimerTask* task, int msec) 	= 0;
	virtual bool removeTask(XTimerTask* task) 			= 0;

	bool addTask(const XTimerTaskPtr& task, int msec)		{ return addTask(task.get(), msec); }
	bool replaceTask(const XTimerTaskPtr& task, int msec)		{ return replaceTask(task.get(), msec); }
	bool replaceTaskLaterThan(const XTimerTaskPtr& task, int msec) 	{ return replaceTaskLaterThan(task.get(), msec); }
	bool removeTask(const XTimerTaskPtr& task)			{ return removeTask(task.get()); }
};


template<class T>
class XTimerTaskImpl0: public XTimerTask
{
	XPtr<T> _obj;
	int (T::*_mfun)();

	friend class XTimerTask;

	XTimerTaskImpl0(T *obj, int (T::*mfun)())
		: _obj(obj), _mfun(mfun)
	{
	}

	virtual void runTimerTask(const XTimerPtr& timer)
	{
		int msec = (_obj.get()->*_mfun)();
		if (msec > 0)
			timer->addTask(this, msec);
	}
};

template <class T>
inline XTimerTaskPtr XTimerTask::create(T *obj, int (T::*mfun)())
{
	return XTimerTaskPtr(new XTimerTaskImpl0<T>(obj, mfun));
}

template <class T>
inline XTimerTaskPtr XTimerTask::create(const XPtr<T>& obj, int (T::*mfun)())
{
	return XTimerTaskPtr(new XTimerTaskImpl0<T>(obj.get(), mfun));
}


template <typename F, typename P1>
class XTimerTaskImpl1: public XTimerTask
{
	F _fun;
	const P1 _p1;

	friend class XTimerTask;

	XTimerTaskImpl1(F fun, P1 p1)
		: _fun(fun), _p1(p1) 
	{
	}

	virtual void runTimerTask(const XTimerPtr& timer)
	{
		_fun(_p1);
	}
};

template <typename F, typename P1>
inline XTimerTaskPtr XTimerTask::create(F fun, P1 p1)
{
	return XTimerTaskPtr(new XTimerTaskImpl1<F,P1>(fun, p1));
}


#endif
