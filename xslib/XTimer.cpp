#include "XTimer.h"
#include "msec.h"
#include "xnet.h"
#include "XLock.h"
#include "XHeap.h"
#include "XThread.h"
#include "xlog.h"
#include "rdtsc.h"
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#define MAX_TIMEOUT	(3600*1000)
#define STACK_SIZE	(1024*1024)

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: XTimer.cpp,v 1.13 2015/04/08 02:51:04 gremlin Exp $";
#endif


XTimerTask::XTimerTask()
	: _index(INT_MIN), _msec(0)
{
}

XTimerTask::~XTimerTask()
{
}

class XTimerTaskMan
{
public:
	XTimerTask *task;

public:
	XTimerTaskMan(XTimerTask* task)	{ this->task = task; }

	int index() const		{ return task->_index; }
	int64_t msec() const		{ return task->_msec; }

	void setIndex(int index)	{ task->_index = index; }
	void setMsec(int64_t msec)	{ task->_msec = msec; }

	bool greater_than(const XTimerTaskMan& r) const
	{
		return task->_msec > r.task->_msec;
	}

	void swap(XTimerTaskMan& r)
	{
		XTimerTask *tmp = task;
		task = r.task;
		r.task = tmp;
		int i = task->_index;
		task->_index = r.task->_index;
		r.task->_index = i;
	}
};

struct XTimerTaskManGreater
{
	bool operator()(const XTimerTaskMan& t1, const XTimerTaskMan& t2)
	{
		return t1.greater_than(t2);
	}
};

struct XTimerTaskManSwap
{
public:
	void operator()(XTimerTaskMan& t1, XTimerTaskMan& t2)
	{
		t1.swap(t2);
	}
};

class XTimerI: public XTimer, private XRecMutex
{
	XHeap<XTimerTaskMan, XTimerTaskManGreater, XTimerTaskManSwap> _heap;
	mutable bool _canceled;
	XThreadPtr _thr;
	int _inter_rfd;
	int _inter_wfd;
	struct pollfd _pollfds[1];

public:
	XTimerI();
	virtual ~XTimerI();

	virtual void start();
	virtual void waitForCancel();
	virtual void cancel();
	virtual void runAllWaitingTasks();

	virtual int64_t msecMonotonic() const;

	virtual bool addTask(XTimerTask* task, int timeout);
	virtual bool replaceTask(XTimerTask* task, int timeout);
	virtual bool replaceTaskLaterThan(XTimerTask* task, int timeout);
	virtual bool removeTask(XTimerTask* task);

public:
	void wait_thread();
};

XTimerI::XTimerI()
{
	_canceled = false;

	int fds[2];
	if (pipe(fds) < 0)
	{
		throw XERROR_FMT(XError, "pipe() failed, errno=%d %m", errno);
	}

	_inter_rfd = fds[0];
	_inter_wfd = fds[1];
	xnet_set_nonblock(_inter_rfd);
	xnet_set_nonblock(_inter_wfd);
	_pollfds[0].fd = _inter_rfd;
	_pollfds[0].events = POLLIN | POLLPRI;
}

XTimerI::~XTimerI()
{
	while (!_heap.empty())
	{
		XTimerTaskMan tm = _heap.pop();
		tm.task->xref_dec();
	}

	::close(_inter_rfd);
	::close(_inter_wfd);
}

bool XTimerI::addTask(XTimerTask* task, int timeout)
{
	if (!task || timeout < 0)
		return false;

	int64_t msec = exact_mono_msec() + timeout;
	XTimerTaskMan tm(task);
	bool interrupt = false;
	{
		Lock lock(*this);
		if (_canceled || tm.index() >= 0)
			return false;

		if (_heap.empty() || _heap.top().msec() > tm.msec())
			interrupt = true;

		tm.setMsec(msec);
		tm.setIndex(_heap.size());
		_heap.push(tm);
		task->xref_inc();
	}

	if (interrupt)
		::write(_inter_wfd, "I", 1);
	return true;
}

bool XTimerI::replaceTask(XTimerTask* task, int timeout)
{
	if (!task || timeout < 0)
		return false;

	int64_t msec = exact_mono_msec() + timeout;
	XTimerTaskMan tm(task);
	bool interrupt = false;
	{
		Lock lock(*this);
		if (_canceled)
			return false;

		if (_heap.empty() || _heap.top().msec() > msec)
			interrupt = true;

		int idx = tm.index();
		if (idx >= 0)	/* modify */
		{
			if (idx >= (int)_heap.size() || _heap[idx].task != task)
				return false;

			if (tm.msec() == msec)
				return true;
			tm.setMsec(msec);
			_heap.fix(idx);
		}
		else	/* add */
		{
			tm.setMsec(msec);
			tm.setIndex(_heap.size());
			_heap.push(tm);
			task->xref_inc();
		}
	}

	if (interrupt)
		::write(_inter_wfd, "I", 1);
	return true;
}

bool XTimerI::replaceTaskLaterThan(XTimerTask* task, int timeout)
{
	if (!task || timeout < 0)
		return false;

	int64_t msec = exact_mono_msec() + timeout;
	XTimerTaskMan tm(task);
	bool interrupt = false;
	{
		Lock lock(*this);
		if (_canceled)
			return false;

		if (_heap.empty() || _heap.top().msec() > msec)
			interrupt = true;

		int idx = tm.index();
		if (idx >= 0)	/* modify */
		{
			if (idx >= (int)_heap.size() || _heap[idx].task != task)
				return false;

			if (tm.msec() <= msec)
				return true;
			tm.setMsec(msec);
			_heap.fix(idx);
		}
		else	/* add */
		{
			tm.setMsec(msec);
			tm.setIndex(_heap.size());
			_heap.push(tm);
			task->xref_inc();
		}
	}

	if (interrupt)
		::write(_inter_wfd, "I", 1);
	return true;
}

bool XTimerI::removeTask(XTimerTask* task)
{
	if (!task)
		return false;

	XTimerTaskMan tm(task);
	{
		Lock lock(*this);
		int idx = tm.index();
		if (idx < 0)
			return false;

		if (idx < (int)_heap.size() && _heap[idx].task == task)
		{
			_heap.erase(idx);
			tm.setIndex(-1);
			task->xref_dec();
		}
	}

	return true;
}

void XTimerI::start()
{
	Lock lock(*this);
	if (!_thr)
	{
		_thr = XThread::create(this, &XTimerI::wait_thread, true, STACK_SIZE);
	}
}

void XTimerI::waitForCancel()
{
	if (!_thr)
		start();

	XThreadPtr thr;
	{
		Lock lock(*this);
		thr = _thr;
	}
	thr->join();
}

void XTimerI::cancel()
{
	Lock lock(*this);
	if (!_canceled)
	{
		_canceled = true;
		::write(_inter_wfd, "I", 1);
	}
}

void XTimerI::runAllWaitingTasks()
{
	Lock lock(*this);
	size_t size = _heap.size();
	if (size > 0)
	{
		int64_t current_msec = exact_mono_msec();
		for (size_t i = 0; i < size; ++i)
		{
			_heap[i].setMsec(current_msec);
		}
		::write(_inter_wfd, "I", 1);
	}
}

int64_t XTimerI::msecMonotonic() const
{
	return exact_mono_msec();
}

void XTimerI::wait_thread()
{
	XTimerPtr me(this);
	while (!_canceled)
	{
		int64_t now = exact_mono_msec();
		int64_t timeout = MAX_TIMEOUT;

		{
			Lock lock(*this);
			if (!_heap.empty())
			{
				timeout = _heap.top().msec() - now;
			}
		}

		if (timeout > 0)
		{
			if (timeout > MAX_TIMEOUT)
				timeout = MAX_TIMEOUT;

			int rc = poll(_pollfds, 1, timeout);
			if (rc > 0)
			{
				char buf[256];
				while (::read(_pollfds[0].fd, buf, sizeof(buf)) > 0)
				{
					continue;
				}
			}
			else if (rc < 0)
			{
				if (errno != EINTR)
					xlog(XLOG_ALERT, "poll()=%d, errno=%d, %m", rc, errno);
			}
		}

		now = exact_mono_msec();

		while (true)
		{
			XTimerTask *task = NULL;
			{
				Lock lock(*this);
				if (_heap.empty() || _heap.top().msec() > now)
					break;

				XTimerTaskMan tm = _heap.pop();
				tm.setIndex(-1);
				task = tm.task;
			}

			try
			{
				task->runTimerTask(me);
			}
			catch (std::exception& ex)
			{
				XError* x = dynamic_cast<XError*>(&ex);
				xlog(XLOG_ERROR, "EXCEPTION: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
			}
			task->xref_dec();
		}
	}

	// The timer is canceled, no others will operate _heap.
	// Need not lock.
	while (!_heap.empty())
	{
		XTimerTaskMan tm = _heap.pop();
		tm.task->xref_dec();
	}
}

XTimerPtr XTimer::create()
{
	return XTimerPtr(new XTimerI());
}

