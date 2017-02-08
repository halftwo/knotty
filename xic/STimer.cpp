#include "STimer.h"
#include "sthread.h"
#include "xslib/xlog.h"
#include "xslib/XHeap.h"
#include "xslib/rdtsc.h"
#include <st.h>
#include <time.h>
#include <limits.h>

#define MAX_TIMEOUT	(3600*1000)

static uint64_t last_mono_tsc;
static int64_t last_mono_msec;

static int64_t get_mono_msec()
{
	struct timespec now;
	uint64_t tsc = rdtsc();
	uint64_t delta = tsc - last_mono_tsc;
	uint64_t freq = cpu_frequency();
	if (delta * 10000 < freq)
	{
		return last_mono_msec;
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	last_mono_tsc = tsc;
	last_mono_msec = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
	return last_mono_msec;
}

STimerTask::STimerTask()
	: _index(INT_MIN), _msec(0)
{
}

STimerTask::~STimerTask()
{
}

struct STimerTaskMan
{
	STimerTask *task;

public:
	STimerTaskMan(STimerTask* task)	{ this->task = task; }

	int index() const		{ return task->_index; }
	int64_t msec() const		{ return task->_msec; }

	void setIndex(int index)	{ task->_index = index; }
	void setMsec(int64_t msec)	{ task->_msec = msec; }

	bool greater_than(const STimerTaskMan& r) const
	{
		return task->_msec > r.task->_msec;
	}

	void swap(STimerTaskMan& r)
	{
		STimerTask *tmp = task;
		task = r.task;
		r.task = tmp;
		int i = task->_index;
		task->_index = r.task->_index;
		r.task->_index = i;
	}
};

struct STimerTaskManGreater
{
	bool operator()(const STimerTaskMan& t1, const STimerTaskMan& t2)
	{
		return t1.greater_than(t2);
	}
};

struct STimerTaskManSwap
{
public:
	void operator()(STimerTaskMan& t1, STimerTaskMan& t2)
	{
		t1.swap(t2);
	}
};

class STimerI: public STimer
{
	XHeap<STimerTaskMan, STimerTaskManGreater, STimerTaskManSwap> _heap;
	bool _canceled;
	st_thread_t _thr;
public:
	STimerI();
	virtual ~STimerI();

	virtual void start();
	virtual void waitForCancel();
	virtual void cancel();

	virtual int64_t msecMonotonic() const;

	virtual bool addTask(STimerTask* task, int timeout);
	virtual bool replaceTask(STimerTask* task, int timeout);
	virtual bool replaceTaskLaterThan(STimerTask* task, int timeout);
	virtual bool removeTask(STimerTask* task);

private:
	void wait_fiber();
};

STimerI::STimerI()
{
	_canceled = false;
	_thr = NULL;
}

STimerI::~STimerI()
{
	_canceled = true;
	if (_thr)
		st_thread_interrupt(_thr);

	while (!_heap.empty())
	{
		STimerTaskMan tm = _heap.pop();
		tm.task->xref_dec();
	}
}

bool STimerI::addTask(STimerTask* task, int timeout)
{
	if (!task || timeout < 0)
		return false;

	int64_t msec = get_mono_msec() + timeout;
	STimerTaskMan tm(task);
	bool interrupt = false;
	{
		if (_canceled || tm.index() >= 0)
			return false;

		if (_heap.empty() || _heap.top().msec() > tm.msec())
			interrupt = true;

		tm.setMsec(msec);
		tm.setIndex(_heap.size());
		_heap.push(tm);
		task->xref_inc();
	}

	if (interrupt && _thr)
		st_thread_interrupt(_thr);
	return true;
}

bool STimerI::replaceTask(STimerTask* task, int timeout)
{
	if (!task || timeout < 0)
		return false;

	int64_t msec = get_mono_msec() + timeout;
	STimerTaskMan tm(task);
	bool interrupt = false;
	{
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

	if (interrupt && _thr)
		st_thread_interrupt(_thr);
	return true;
}

bool STimerI::replaceTaskLaterThan(STimerTask* task, int timeout)
{
	if (!task || timeout < 0)
		return false;

	int64_t msec = get_mono_msec() + timeout;
	STimerTaskMan tm(task);
	bool interrupt = false;
	{
		if (_canceled)
			return false;

		if (_heap.empty() || _heap.top().msec() > msec);
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

	if (interrupt && _thr)
		st_thread_interrupt(_thr);
	return true;
}

bool STimerI::removeTask(STimerTask* task)
{
	if (!task)
		return false;

	STimerTaskMan tm(task);
	{
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

void STimerI::start()
{
	if (!_thr)
	{
		_thr = sthread_create(this, &STimerI::wait_fiber, true);
	}
}

void STimerI::waitForCancel()
{
	if (!_thr)
		start();

	st_thread_t thr = _thr;
	st_thread_join(thr, NULL);
}

void STimerI::cancel()
{
	if (!_canceled)
	{
		_canceled = true;
		if (_thr)
			st_thread_interrupt(_thr);
	}
}

int64_t STimerI::msecMonotonic() const
{
	return get_mono_msec();
}

void STimerI::wait_fiber()
{
	STimerPtr me(this);
	int64_t now = get_mono_msec();
	while (!_canceled)
	{
		int64_t timeout = _heap.empty() ? MAX_TIMEOUT : (_heap.top().msec() - now);
		if (timeout > 0)
		{
			st_usleep(timeout*1000LL);
			now = get_mono_msec();
		}

		if (!_heap.empty() && _heap.top().msec() <= now)
		{
			do
			{
				STimerTaskMan tm = _heap.pop();
				tm.setIndex(-1);
				try
				{
					tm.task->runTimerTask(me);
				}
				catch (std::exception& ex)
				{
					XError* x = dynamic_cast<XError*>(&ex);
					xlog(XLOG_CRIT, "EXCEPTION: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
				}
				tm.task->xref_dec();
			} while (!_heap.empty() && _heap.top().msec() <= now);
			now = get_mono_msec();
		}
	}
	_thr = NULL;
}

STimerPtr STimer::create()
{
	return STimerPtr(new STimerI());
}

