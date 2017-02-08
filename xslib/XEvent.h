/* $Id: XEvent.h,v 1.13 2012/10/19 10:01:23 jiagui Exp $ */
#ifndef XEvent_h_
#define XEvent_h_

#include "XRefCount.h"
#include "xlog.h"
#include <stdint.h>

namespace XEvent
{

class FdHandler;
class TaskHandler;
class SignalHandler;
class Dispatcher;

typedef class XPtr<FdHandler> 		FdHandlerPtr;
typedef class XPtr<TaskHandler> 	TaskHandlerPtr;
typedef class XPtr<SignalHandler> 	SignalHandlerPtr;
typedef class XPtr<Dispatcher> 		DispatcherPtr;

class FdQueue;
class FdMan;
class TaskMan;
class SignalMan;
class EventMan;


class EventHandler: virtual public XRefCount
{
public:
	virtual ~EventHandler() 			{}
	bool event_concurrent() const 			{ return _lastEvent == (EventMan *)-1; }

protected:
	EventHandler(): _lastEvent(0) 			{}
	EventHandler(bool concurrent): _lastEvent((EventMan*)(intptr_t)(concurrent?-1:0)) {}

private:
	friend class EventMan;
	EventMan* _lastEvent;
};


enum
{
	READ_EVENT 	= 0x0001,
	WRITE_EVENT	= 0x0002,
	CLOSE_EVENT	= 0x0004,
	EDGE_TRIGGER	= 0x0100,
	ONE_SHOT	= 0x0200,
};

class FdHandler: virtual public EventHandler
{
public:
	FdHandler();
	virtual ~FdHandler();

	virtual void event_on_fd(const DispatcherPtr& dispatcher, int events) = 0;

private:
	friend class EventMan;
	friend class FdMan;
	friend class FdQueue;
	int _index;
	int _efd;
	int _eventsAsk;	// asked events
	int _eventsGet;	// got events
	int64_t _id;
	FdHandler *_next, **_pprev;
};


class TaskHandler: virtual public EventHandler
{
public:
	template <class T>
	static TaskHandlerPtr create(T *obj, int (T::*mfun)());

	template <class T>
	static TaskHandlerPtr create(const XPtr<T>& obj, int (T::*mfun)());

	TaskHandler();
	virtual ~TaskHandler();

	virtual void event_on_task(const DispatcherPtr& dispatcher) 	= 0;

private:
	friend class EventMan;
	friend class TaskMan;
	int _index;
	int64_t _msec;
	int64_t _id;
};


class SignalHandler: virtual public EventHandler
{
public:
	SignalHandler();
	virtual ~SignalHandler();

	virtual void event_on_signal(const DispatcherPtr& dispatcher) 	= 0;

private:
	friend class EventMan;
	friend class SignalMan;
	int _esig;
};


class Dispatcher: virtual public XRefCount
{
public:
	static DispatcherPtr create(const char *kind = 0);

	virtual ~Dispatcher();

	virtual void start() 						= 0;
	virtual void waitForCancel() 					= 0;
	virtual void cancel() 						= 0;

	virtual bool canceled() const 					= 0;
	virtual int64_t msecRealtime() const 				= 0;
	virtual int64_t msecMonotonic() const 				= 0;

	virtual void setThreadPool(size_t threadMin, size_t threadMax, size_t stackSize) 	= 0;
	virtual void getThreadPool(size_t *threadMin, size_t *threadMax, size_t *stackSize) 	= 0;

	virtual size_t countThread() const 				= 0;
	virtual size_t countFd() const 					= 0;
	virtual size_t countTask() const 				= 0;

	virtual bool addFd(FdHandler* hr, int fd, int events) 		{ return false; }
	virtual bool replaceFd(FdHandler* hr, int fd, int events) 	{ return false; }
	virtual bool removeFd(FdHandler* hr) 				{ return false; }
	virtual bool readyFd(FdHandler* hr, int events) 		{ return false; }

	virtual bool addTask(TaskHandler* hr, int msec) 		{ return false; }
	virtual bool replaceTask(TaskHandler* hr, int msec) 		{ return false; }
	virtual bool replaceTaskLaterThan(TaskHandler* hr, int msec) 	{ return false; }
	virtual bool removeTask(TaskHandler* hr) 			{ return false; }

	virtual bool addSignal(SignalHandler* hr, int sig) 		{ return false; }
	virtual bool removeSignal(SignalHandler* hr) 			{ return false; }


	bool addFd(const FdHandlerPtr& hr, int fd, int events) 		{ return addFd(hr.get(), fd, events); }
	bool replaceFd(const FdHandlerPtr& hr, int fd, int events)	{ return replaceFd(hr.get(), fd, events); }
	bool removeFd(const FdHandlerPtr& hr) 				{ return removeFd(hr.get()); }
	bool readyFd(const FdHandlerPtr& hr, int events)		{ return readyFd(hr.get(), events); }

	bool addTask(const TaskHandlerPtr& hr, int msec) 		{ return addTask(hr.get(), msec); }
	bool replaceTask(const TaskHandlerPtr& hr, int msec) 		{ return replaceTask(hr.get(), msec); }
	bool replaceTaskLaterThan(const TaskHandlerPtr& hr, int msec) 	{ return replaceTaskLaterThan(hr.get(), msec); }
	bool removeTask(const TaskHandlerPtr& hr) 			{ return removeTask(hr.get()); }

	bool addSignal(const SignalHandlerPtr& hr, int sig) 		{ return addSignal(hr.get(), sig); }
	bool removeSignal(const SignalHandlerPtr& hr) 			{ return removeSignal(hr.get()); }
};



template<class T>
struct TaskHandlerImpl: public TaskHandler
{
	XPtr<T> _obj;
	int (T::*_mfun)();

	friend class TaskHandler;

	TaskHandlerImpl(T *obj, int (T::*mfun)())
		: _obj(obj), _mfun(mfun)
	{
	}

	virtual void event_on_task(const DispatcherPtr& dispatcher)
	{
		int msec = (_obj.get()->*_mfun)();
		if (msec > 0)
			dispatcher->addTask(this, msec);
	}
};

template <class T>
inline TaskHandlerPtr TaskHandler::create(T *obj, int (T::*mfun)())
{
	return TaskHandlerPtr(new TaskHandlerImpl<T>(obj, mfun));
}

template <class T>
inline TaskHandlerPtr TaskHandler::create(const XPtr<T>& obj, int (T::*mfun)())
{
	return TaskHandlerPtr(new TaskHandlerImpl<T>(obj.get(), mfun));
}


};	// namespace XEvent

#endif
