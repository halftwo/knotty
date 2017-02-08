#include "XThread.h"
#include "XError.h"
#include "XLock.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: XThread.cpp,v 1.4 2012/02/22 02:05:14 jiagui Exp $";
#endif

XThreadPtr XThread::create_thread(void *(*routine)(void *), void *arg, bool joinable, size_t stackSize)
{
	ID id;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, joinable ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED);
	if (stackSize > 0)
		pthread_attr_setstacksize(&attr, stackSize);

	int rc = pthread_create(&id, &attr, routine, arg);
	pthread_attr_destroy(&attr);

	if (rc != 0)
		throw XERROR_FMT(XThreadSyscallError, "pthread_create()=%d", rc);

	return XThreadPtr(new XThread(id, joinable));
}

XThread::XThread(ID id, bool joinable)
	: _id(id), _joinable(joinable)
{
}

void* XThread::join()
{
	if (!_joinable)
		throw XERROR_FMT(XThreadError, "XThread already detached");

	void *value;
	int rc = pthread_join(_id, &value);
	if (rc != 0)
		throw XERROR_FMT(XThreadSyscallError, "pthread_join()=%d", rc);

	_joinable = false;
	return value;
}

void XThread::detach()
{
	if (!_joinable)
		throw XERROR_FMT(XThreadError, "XThread already detached");

	int rc = pthread_detach(_id);
	if (rc != 0)
		throw XERROR_FMT(XThreadSyscallError, "pthread_detach()=%d", rc);

	_joinable = false;
}

