/* $Id: XLock.h,v 1.15 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Some thread synchronization classes.

   Author: XIONG Jiagui
   Date: 2007-01-23
 */
#ifndef XLock_h_
#define XLock_h_

#include "XError.h"
#include <pthread.h>


XE_(XLogicError, 	XThreadDeadlockError)
XE_(XLogicError, 	XThreadCondError)
XE_(XError,		XThreadSyscallError)


class XMutex;
class XRecMutex;
class XCond;

template<typename T> class XLock;
template<typename T> class XTryLock;
template<typename T> class XMonitor;


template<typename T>
class XLock
{
public:
	XLock(const T& mutex): _mutex(mutex) { _mutex.lock(); _acquired = true; }

	~XLock() { if (_acquired) _mutex.unlock(); }

	void acquire() const { if (!_acquired) { _mutex.lock(); _acquired = true; } }

	void release() const { if (_acquired) { _mutex.unlock(); _acquired = false; } }

	bool tryAcquire() const { if (!_acquired) _acquired = _mutex.trylock(); return _acquired; }

	bool acquired() const { return _acquired; }

protected:
	// XTryLock's constructors
	XLock(const T& mutex, int): _mutex(mutex) { _acquired = _mutex.trylock(); }

private:
	XLock();
	XLock& operator=(const XLock&);

	friend class XCond;
	const T& _mutex;
	mutable bool _acquired;
};


template<typename T>
class XTryLock: public XLock<T>
{
public:
	XTryLock(const T& mutex): XLock<T>(mutex, 0) {}

private:
	XTryLock();
	XTryLock& operator=(const XTryLock&);
	friend class XCond;
};


class XMutex
{
public:
	typedef XLock<XMutex> Lock;
	typedef XTryLock<XMutex> TryLock;

	XMutex();
	~XMutex();

	void lock() const;
	void unlock() const;
	bool trylock() const;

private:
	XMutex(const XMutex&);
	XMutex& operator=(const XMutex&);

	template<typename T> friend class XMonitor;
	friend class XCond;

	mutable pthread_mutex_t _mutex;
};


class XRecMutex
{
public:
	typedef XLock<XRecMutex> Lock;
	typedef XTryLock<XRecMutex> TryLock;

	XRecMutex();
	~XRecMutex();

	void lock() const;
	void unlock() const;
	bool trylock() const;

private:
	XRecMutex(const XRecMutex&);
	XRecMutex& operator=(const XRecMutex&);

	template<typename T> friend class XMonitor;
	friend class XCond;

	mutable pthread_mutex_t _mutex;
};


class XCond
{
public:
	XCond() { pthread_cond_init(&_cond, NULL); }
	~XCond() { pthread_cond_destroy(&_cond); }

	void signal() const { pthread_cond_signal(&_cond); }
	void broadcast() const { pthread_cond_broadcast(&_cond); }

	template<typename Lock> 
	void wait(const Lock& lock) const
	{
		if (!lock.acquired())
			throw XERROR_MSG(XThreadCondError, "no lock acquired when calling wait() in XCond");

		pthread_cond_wait(&_cond, &lock._mutex._mutex);
	}

	template <typename Lock> 
	bool timedwait(const Lock& lock, const struct timespec *abstime) const
	{
		if (!lock.acquired())
			throw XERROR_MSG(XThreadCondError, "no lock acquired when calling timedwait() in XCond");

		return (pthread_cond_timedwait(&_cond, &lock._mutex._mutex, abstime) == 0);
	}

private:
	XCond(const XCond&);
	XCond& operator=(const XCond& r);

	mutable pthread_cond_t _cond;
};


template<typename T>
class XMonitor
{
public:

	typedef XLock<XMonitor<T> > Lock;
	typedef XTryLock<XMonitor<T> > TryLock;

	XMonitor() { pthread_cond_init(&_cond, NULL); }
	~XMonitor() { pthread_cond_destroy(&_cond); }

	void lock() const { _mutex.lock(); }
	void unlock() const { _mutex.unlock(); }
	bool trylock() const { return _mutex.trylock(); }

	void wait() const { pthread_cond_wait(&_cond, &_mutex._mutex); }

	bool timedwait(const struct timespec *abstime) const
	{
		return pthread_cond_timedwait(&_cond, &_mutex._mutex, abstime) == 0;
	}

	void notify() const { pthread_cond_signal(&_cond); }
	void notifyAll() const { pthread_cond_broadcast(&_cond); }

private:
	XMonitor(const XMonitor&);
	XMonitor& operator=(const XMonitor& r);

	T _mutex;
	mutable pthread_cond_t _cond;
};

#endif

