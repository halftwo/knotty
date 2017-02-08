#ifndef STimer_h_
#define STimer_h_


#include "xslib/XRefCount.h"
#include <stdint.h>


class STimer;
class STimerTask;
class STimerTaskMan;

typedef XPtr<STimer> STimerPtr;
typedef XPtr<STimerTask> STimerTaskPtr;


class STimerTask: virtual public XRefCount
{
public:
	template<class T>
	static STimerTaskPtr create(T *obj, int (T::*mfun)());

	template<class T>
	static STimerTaskPtr create(const XPtr<T>& obj, int (T::*mfun)());

	template <typename F, typename P1>
	static STimerTaskPtr create(F fun, P1 p1);

	STimerTask();
	virtual ~STimerTask();

	int64_t msecRun() const			{ return _msec; }

	virtual void runTimerTask(const STimerPtr& timer) = 0;

private:
	friend class STimerTaskMan;
	int _index;
	int64_t _msec;
};


class STimer: virtual public XRefCount
{
public:
	static STimerPtr create();

	virtual void start() 						= 0;
	virtual void waitForCancel() 					= 0;
	virtual void cancel() 						= 0;

	virtual int64_t msecMonotonic() const 				= 0;

	virtual bool addTask(STimerTask* task, int msec) 		= 0;
	virtual bool replaceTask(STimerTask* task, int msec) 		= 0;
	virtual bool replaceTaskLaterThan(STimerTask* task, int msec) 	= 0;
	virtual bool removeTask(STimerTask* task) 			= 0;

	bool addTask(const STimerTaskPtr& task, int msec)		{ return addTask(task.get(), msec); }
	bool replaceTask(const STimerTaskPtr& task, int msec)		{ return replaceTask(task.get(), msec); }
	bool replaceTaskLaterThan(const STimerTaskPtr& task, int msec) 	{ return replaceTaskLaterThan(task.get(), msec); }
	bool removeTask(const STimerTaskPtr& task)			{ return removeTask(task.get()); }
};


template<class T>
class STimerTaskImpl0: public STimerTask
{
	XPtr<T> _obj;
	int (T::*_mfun)();

	friend class STimerTask;

	STimerTaskImpl0(T *obj, int (T::*mfun)())
		: _obj(obj), _mfun(mfun)
	{
	}

	virtual void runTimerTask(const STimerPtr& timer)
	{
		int msec = (_obj.get()->*_mfun)();
		if (msec > 0)
			timer->addTask(this, msec);
	}
};

template <class T>
inline STimerTaskPtr STimerTask::create(T *obj, int (T::*mfun)())
{
	return STimerTaskPtr(new STimerTaskImpl0<T>(obj, mfun));
}

template <class T>
inline STimerTaskPtr STimerTask::create(const XPtr<T>& obj, int (T::*mfun)())
{
	return STimerTaskPtr(new STimerTaskImpl0<T>(obj.get(), mfun));
}


template <typename F, typename P1>
class STimerTaskImpl1: public STimerTask
{
	F _fun;
	const P1 _p1;

	friend class STimerTask;

	STimerTaskImpl1(F fun, P1 p1)
		: _fun(fun), _p1(p1) 
	{
	}

	virtual void runTimerTask(const STimerPtr& timer)
	{
		_fun(_p1);
	}
};

template <typename F, typename P1>
inline STimerTaskPtr STimerTask::create(F fun, P1 p1)
{
	return STimerTaskPtr(new STimerTaskImpl1<F,P1>(fun, p1));
}


#endif
