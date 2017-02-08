#include "XLock.h"
#include <assert.h>
#include <errno.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: XLock.cpp,v 1.8 2010/03/02 04:12:57 jiagui Exp $";
#endif

#define THROW_ERROR(rc, func)	throw_Mutex_Error(__FILE__, __LINE__, rc, func)

static void throw_Mutex_Error(const char *file, int line, int rc, const char *func)
{
	if (rc == EDEADLK)
		throw XERROR(XThreadDeadlockError);
	throw XERROR_CODE_MSG(XThreadSyscallError, rc, func);
}

static pthread_mutexattr_t _xattr_;
static pthread_mutexattr_t _xrecattr_;

static int init_xattr()
{
	int rc = pthread_mutexattr_init(&_xattr_);
	if (rc == 0)
		rc = pthread_mutexattr_settype(&_xattr_, PTHREAD_MUTEX_ERRORCHECK_NP);
	return rc;
}

static int init_xrecattr()
{
	int rc = pthread_mutexattr_init(&_xrecattr_);
	if (rc == 0)
		rc = pthread_mutexattr_settype(&_xrecattr_, PTHREAD_MUTEX_RECURSIVE_NP);
	return rc;
}

static int _xattr_init_ = init_xattr();
static int _xrecattr_init_ = init_xrecattr();


XMutex::XMutex()
{
	if (_xattr_init_)
		THROW_ERROR(_xattr_init_, "pthread_mutexattr_init or pthread_mutexattr_settype");

	int rc = pthread_mutex_init(&_mutex, &_xattr_);
	if (rc)
		THROW_ERROR(rc, "pthread_mutex_init");
}

XMutex::~XMutex()
{
	pthread_mutex_destroy(&_mutex);
}

void XMutex::lock() const
{
	int rc = pthread_mutex_lock(&_mutex);
	if (rc)
	{
		THROW_ERROR(rc, "pthread_mutex_lock");
	}
}

bool XMutex::trylock() const
{
	int rc = pthread_mutex_trylock(&_mutex);
	if (rc)
	{
		if (rc == EBUSY)
			return false;

		THROW_ERROR(rc, "pthread_mutex_trylock");
	}
	return true;
}

void XMutex::unlock() const
{
	int rc = pthread_mutex_unlock(&_mutex);
	if (rc)
	{
		THROW_ERROR(rc, "pthread_mutex_unlock");
	}
}


XRecMutex::XRecMutex()
{
	if (_xrecattr_init_)
		THROW_ERROR(_xrecattr_init_, "pthread_mutexattr_init or pthread_mutexattr_settype");

	int rc = pthread_mutex_init(&_mutex, &_xrecattr_);
	if (rc)
		THROW_ERROR(rc, "pthread_mutex_init");
}

XRecMutex::~XRecMutex()
{
	pthread_mutex_destroy(&_mutex);
}

void XRecMutex::lock() const
{
	int rc = pthread_mutex_lock(&_mutex);
	if (rc)
		THROW_ERROR(rc, "pthread_mutex_lock");
}

bool XRecMutex::trylock() const
{
	int rc = pthread_mutex_trylock(&_mutex);
	if (rc)
	{
		if (rc == EBUSY)
			return false;

		THROW_ERROR(rc, "pthread_mutex_trylock");
	}
	return true;
}

void XRecMutex::unlock() const
{
	int rc = pthread_mutex_unlock(&_mutex);
	if (rc)
	{
		THROW_ERROR(rc, "pthread_mutex_unlock");
	}
}


#ifdef TEST_XLOCK

int main()
{
	XMonitor<XRecMutex> m;
	XMonitor<XRecMutex>::Lock sync(m);
	{
		XMonitor<XRecMutex>::Lock sync2(m);
	}
	return 0;
}

#endif
