/* $Id: XRWLock.h,v 1.5 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2007-06-14
 */
#ifndef XRWLock_h_
#define XRWLock_h_

#include <pthread.h>


class XRWMutex;
template <typename T> class XRLock;
template <typename T> class XTryRLock;
template <typename T> class XWLock;
template <typename T> class XTryWLock;


template <typename T>
class XRLock
{
public:
	XRLock(const T& mutex): _mutex(mutex)
	{ 
		_mutex.rdlock();
		_acquired = true;
	}

	~XRLock()
	{
		 if (_acquired) _mutex.unlock();
	}

	void acquire() const
	{
		if (!_acquired) { _mutex.rdlock(); _acquired = true; }
	}

	bool tryAcquire() const
	{
		if (!_acquired) _acquired = _mutex.tryrdlock();
		return _acquired;
	}

	bool timedAcquire(const struct timespec * timeout) const
	{
		if (!_acquired) _acquired = _mutex.timedrdlock(timeout);
		return _acquired;
	}

	void release() const
	{
		if (_acquired) { _mutex.unlock(); _acquired = false; }
	}

	bool acquired() const
	{
		return _acquired;
	}


protected:
	// XTryRLock's constructors
	XRLock(const T& mutex, int): _mutex(mutex)
	{
		_acquired = _mutex.tryrdlock();
	}

	XRLock(const T& mutex, const struct timespec *timeout): _mutex(mutex)
	{
		_acquired = _mutex.timedrdLock(timeout);
	}

private:
	XRLock(const XRLock&);
	XRLock& operator=(const XRLock&);

	const T& _mutex;
	mutable bool _acquired;
};


template <typename T>
class XTryRLock: public XRLock<T>
{
public:
	XTryRLock(const T& mutex): XRLock<T>(mutex, 0) {}

	XTryRLock(const T& mutex, const struct timespec *timeout): XRLock<T>(mutex, timeout) {}
};


template <typename T>
class XWLock
{
public:
	XWLock(const T& mutex): _mutex(mutex)
	{ 
		_mutex.wrlock();
		_acquired = true;
	}

	~XWLock()
	{
		if (_acquired) _mutex.unlock();
	}

	void acquire() const
	{
		if (!_acquired) { _mutex.wrlock(); _acquired = true; }
	}

	bool tryAcquire() const
	{
		if (!_acquired) _acquired = _mutex.trywrlock();
		return _acquired;
	}

	bool timedAcquire(const struct timespec *timeout) const
	{
		if (!_acquired) _acquired = _mutex.timedwrlock(timeout);
		return _acquired;
	}

	void release() const
	{
		if (_acquired) { _mutex.unlock(); _acquired = false; }
	}

	bool acquired() const { return _acquired; }

protected:
	// XTryWLock's constructors
	XWLock(const T& mutex, int): _mutex(mutex)
	{
		_acquired = _mutex.trywrlock();
	}

	XWLock(const T& mutex, const struct timespec *timeout): _mutex(mutex)
	{
		_acquired = _mutex.timedwrlock(timeout);
	}

private:
	XWLock(const XWLock&);
	XWLock& operator=(const XWLock&);

	const T& _mutex;
	mutable bool _acquired;
};


template <typename T>
class XTryWLock: public XWLock<T>
{
public:
	XTryWLock(const T& mutex): XWLock<T>(mutex, true) {}

	XTryWLock(const T& mutex, const struct timespec *timeout): XWLock<T>(mutex, timeout) {}
};


class XRWMutex
{
public:
	typedef XRLock<XRWMutex> RLock;
	typedef XTryRLock<XRWMutex> TryRLock;
	typedef XWLock<XRWMutex> WLock;
	typedef XTryWLock<XRWMutex> TryWLock;


	XRWMutex() { pthread_rwlock_init(&_rwlock, NULL); }

	~XRWMutex() { pthread_rwlock_destroy(&_rwlock); }


	void rdlock() const { pthread_rwlock_rdlock(&_rwlock); }

	bool tryrdlock() const
	{
		return pthread_rwlock_tryrdlock(&_rwlock) == 0;
	}

	bool timedrdlock(const struct timespec *timeout) const
	{
		return pthread_rwlock_timedrdlock(&_rwlock, timeout) == 0;
	}


	void wrlock() const { pthread_rwlock_wrlock(&_rwlock); }

	bool trywrlock() const
	{
		return pthread_rwlock_trywrlock(&_rwlock) == 0;
	}

	bool timedwrlock(const struct timespec *timeout) const
	{ 
		return pthread_rwlock_timedwrlock(&_rwlock, timeout) == 0;
	}


	void unlock() const { pthread_rwlock_unlock(&_rwlock); }

private:
	XRWMutex(const XRWMutex&);
	void operator=(const XRWMutex&);

	mutable pthread_rwlock_t _rwlock;
};


#endif
