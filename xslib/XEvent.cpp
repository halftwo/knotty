#include "XEvent.h"
#include "msec.h"
#include "Enforce.h"
#include "ScopeGuard.h"
#include "rdtsc.h"
#include "obpool.h"
#include "tree.h"
#include "xatomic.h"
#include "heap.h"
#include "XError.h"
#include "XLock.h"
#include "XHeap.h"
#include "xlog.h"
#include "xnet.h"
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <vector>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: XEvent.cpp,v 1.37 2015/04/08 02:51:04 gremlin Exp $";
#endif

#define MAX_TIMEOUT	(3600*1000)		/* in milliseconds */
#define THREAD_INTERVAL	20
#define IDLE_INTERVAL	(60*1000)
#define IDLE_FASTREAP	(10*1000)
#define IDLE_OVERMUCH	60

namespace XEvent
{

static pthread_once_t _once = PTHREAD_ONCE_INIT;

static void initialize()
{
	sigset_t signal_mask;
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGPIPE);
	int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
	if (rc != 0)
		xlog(XLOG_ERR, "pthread_sigmask()=%d", rc);
}


FdHandler::FdHandler()
	: _index(INT_MIN), _efd(INT_MIN), _eventsAsk(0), _eventsGet(0), _id(0), _next(0), _pprev(0)
{
}

FdHandler::~FdHandler()
{
}

TaskHandler::TaskHandler()
	: _index(INT_MIN), _msec(0)
{
}

TaskHandler::~TaskHandler()
{
}

SignalHandler::SignalHandler()
	: _esig(INT_MIN)
{
}

SignalHandler::~SignalHandler()
{
}

class FdQueue
{
	FdHandler *first;
	FdHandler **last;
public:
	FdQueue(): first(0), last(&first) {}
	void add(FdHandler *hr);
	void remove(FdHandler *hr);
	void foreach(void (*fun)(FdHandler *hr));
};

void FdQueue::add(FdHandler *hr)
{
	hr->_next = NULL;
	hr->_pprev = last;
	*last = hr;
	last = &hr->_next;
}

void FdQueue::remove(FdHandler *hr)
{
	if (hr->_next)
		hr->_next->_pprev = hr->_pprev;
	else
		last = hr->_pprev;
	*hr->_pprev = hr->_next;
}

void FdQueue::foreach(void (*fun)(FdHandler *hr))
{
	FdHandler *hr, *next = 0;
	for (hr = first; hr; hr = next)
	{
		next = hr->_next;
		fun(hr);
	}
}

class FdMan
{
public:
	FdHandler *hr;

public:
	FdMan(FdHandler *p): hr(p)
	{
	}

	void turnOn(FdQueue& queue, int fd)
	{
		// assert(fd >= 0);
		queue.add(hr);
		hr->_efd = fd;
	}

	void turnOff(FdQueue& queue, int mark)
	{
		// assert(mark < 0);
		queue.remove(hr);
		hr->_efd = mark;
	}

	void incRef() 			{ hr->xref_inc(); }
	void decRef() 			{ hr->xref_dec(); }

	int efd() const			{ return hr->_efd; }
	int index() const 		{ return hr->_index; }
	int64_t id() const		{ return hr->_id; }
	int eventsAsk() const 		{ return hr->_eventsAsk; }

	void setEventsAsk(int events) 	{ hr->_eventsAsk = events; }
	void setEventsGet(int events) 	{ hr->_eventsGet = events; }
	void combineEventsGet(int events) { hr->_eventsGet |= events; }
	void setIndex(int index) 	{ hr->_index = index; }
	void setId(int64_t id) 		{ hr->_id = id; }

	bool less_than(const FdMan& r) const
	{
		// NB: reversed
		return hr->_id > r.hr->_id;
	}

	void swap(FdMan& r)
	{
		FdHandler *tmp = hr;
		hr = r.hr;
		r.hr = tmp;
		int i = hr->_index;
		hr->_index = r.hr->_index;
		r.hr->_index = i;
	}
};

struct FdManLess
{
	bool operator()(const FdMan& t1, const FdMan& t2)
	{
		return t1.less_than(t2);
	}
};

struct FdManSwap
{
	void operator()(FdMan& t1, FdMan& t2)
	{
		t1.swap(t2);
	}
};


class TaskMan
{
public:
	TaskHandler *hr;

public:
	TaskMan(TaskHandler *p): hr(p)
	{
	}

	int index() const		{ return hr->_index; }
	int64_t msec() const 		{ return hr->_msec; }

	void setIndex(int index) 	{ hr->_index = index; }
	void setMsec(int64_t msec) 	{ hr->_msec = msec; }
	void setId(int64_t id) 		{ hr->_id = id; }

	bool less_than(const TaskMan& r) const
	{
		// NB: reversed
		return (hr->_msec > r.hr->_msec) || (hr->_msec == r.hr->_msec && hr->_id > r.hr->_id);
	}

	void swap(TaskMan& r)
	{
		TaskHandler *tmp = hr;
		hr = r.hr;
		r.hr = tmp;
		int i = hr->_index;
		hr->_index = r.hr->_index;
		r.hr->_index = i;
	}

};

struct TaskManLess
{
	bool operator()(const TaskMan& t1, const TaskMan& t2)
	{
		return t1.less_than(t2);
	}
};

struct TaskManSwap
{
	void operator()(TaskMan& t1, TaskMan& t2)
	{
		t1.swap(t2);
	}
};


class EventMan
{
	enum EventType
	{
		NONE_TYPE 	= 0,
		SIGNAL_TYPE 	= 1,
		FD_TYPE 	= 2,
		TIMEOUT_TYPE 	= 3,
	};

	static obpool_t _pool;
	static XMutex _mutex;

	EventMan* _next;
	EventHandler* _ev_hr;
	union
	{
		SignalHandler* _sg_hr;
		TaskHandler* _tm_hr;
		FdHandler* _fd_hr;
	};
	EventType _type;
	int _events;

	void* operator new(size_t size)
	{
		XMutex::Lock lock(_mutex);
		return obpool_acquire(&_pool);
	}

	void operator delete(void *p)
	{
		XMutex::Lock lock(_mutex);
		obpool_release(&_pool, p);
	}

	EventMan(SignalHandler *hr): _next(0), _ev_hr(hr), _sg_hr(hr), _type(SIGNAL_TYPE) 	{}
	EventMan(FdHandler *hr): _next(0), _ev_hr(hr), _fd_hr(hr), _type(FD_TYPE), _events(_fd_hr->_eventsGet) {}
	EventMan(TaskHandler *hr): _next(0), _ev_hr(hr), _tm_hr(hr), _type(TIMEOUT_TYPE) 	{}
	~EventMan() 										{}

public:
	static EventMan* create(SignalHandler *hr)
	{
		return new EventMan(hr);
	}

	static EventMan* create(FdHandler *hr)
	{
		return new EventMan(hr);
	}

	static EventMan* create(TaskHandler *hr)
	{
		return new EventMan(hr);
	}

	bool can_run()
	{
		bool res = true;
		if (!_ev_hr->event_concurrent())
		{
			XMutex::Lock lock(_mutex);
			if (_ev_hr->_lastEvent)
			{
				res = false;
				_ev_hr->_lastEvent->_next = this;
			}
			_ev_hr->_lastEvent = this;
		}
		return res;
	}

	void doit(const DispatcherPtr& dispatcher)
	{
		try
		{
			switch (_type)
			{
			case SIGNAL_TYPE:
				_sg_hr->event_on_signal(dispatcher);
				break;
			case FD_TYPE:
				_fd_hr->event_on_fd(dispatcher, _events);
				break;
			case TIMEOUT_TYPE:
				_tm_hr->event_on_task(dispatcher);
				break;
			default:
				break;
			}
		}
		catch (std::exception& ex)
		{
			XError* x = dynamic_cast<XError*>(&ex);
			xlog(XLOG_ERR, "EXCEPTION: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
		}
	}

	EventMan* destroy()
	{
		EventHandler* hr = _ev_hr;
		EventMan* next;
		{
			XMutex::Lock lock(_mutex);
			if (_ev_hr->_lastEvent == this)
				_ev_hr->_lastEvent = 0;
			next = _next;
			this->EventMan::~EventMan();
			obpool_release(&_pool, this);
		}

		hr->xref_dec();
		return next;
	}
};

XMutex EventMan::_mutex;
obpool_t EventMan::_pool = OBPOOL_INITIALIZER(sizeof(EventMan));



#define NON_THREAD	((pthread_t)-1)

class EpollDisp: public Dispatcher, private XRecMutex
{
public:
	EpollDisp();
	virtual ~EpollDisp();

	virtual bool addFd(FdHandler* handler, int fd, int events);
	virtual bool replaceFd(FdHandler* handler, int fd, int events);
	virtual bool removeFd(FdHandler* handler);
	virtual bool readyFd(FdHandler* handler, int events);

	virtual bool addTask(TaskHandler* handler, int timeout);
	virtual bool replaceTask(TaskHandler* handler, int timeout);
	virtual bool replaceTaskLaterThan(TaskHandler* handler, int timeout);
	virtual bool removeTask(TaskHandler* handler);

	virtual bool addSignal(SignalHandler* handler, int sig);
	virtual bool removeSignal(SignalHandler* handler);

	virtual void start();
	virtual void waitForCancel();
	virtual void cancel();

	virtual bool canceled() const 		{ return _canceled; }
	virtual int64_t msecRealtime() const 	{ return exact_real_msec(); }
	virtual int64_t msecMonotonic() const 	{ return exact_mono_msec(); }
	virtual size_t countThread() const 	{ return _num_thread; }
	virtual size_t countFd() const 		{ return _fcount; }
	virtual size_t countTask() const 	{ return _tcount; }

	virtual void setThreadPool(size_t threadMin, size_t threadMax, size_t stackSize);
	virtual void getThreadPool(size_t *threadMin, size_t *threadMax, size_t *stackSize);

	void clear_handlers();
	bool new_thread(bool wait);
	void work();
	void wait_and_work();

private:
	EventMan *nextEvent();
	bool _do_add_fd(FdMan& fm, int fd, int events, struct epoll_event& ep);
	void _fd_added(FdMan& fm, int fd);
	void _fd_removed(FdMan& fm, int mark);

private:
	pthread_t _leader;
	XCond _cond;
	XCond _finish_cond;

	std::vector<struct epoll_event> _ep_events;
	FdQueue _fqueue;			// fds in the epoll

	std::vector<FdHandler *> _fd2hr;
	XHeap<FdMan, FdManLess, FdManSwap> _fheap;
	XHeap<TaskMan, TaskManLess, TaskManSwap> _theap;
	size_t _fcount;
	size_t _tcount;

	int64_t _tm_ref_msec;
	int64_t _tm_lastid;
	int64_t _fd_ref_id;
	int64_t _fd_lastid;
	int64_t _idle_mono_msec;

	int _timeout;
	int _inter_rfd;
	int _inter_wfd;
	int _epfd;
	int _num_thread;
	int _min_thread;
	int _max_thread;
	xatomic_t _num_idle;
	bool _started;
	bool _canceled;
	bool _spawning;

	pthread_attr_t _attr;
};

EpollDisp::EpollDisp()
{
	_epfd = epoll_create(256);
	if (_epfd < 0)
		throw XERROR_FMT(XError, "epoll_create() failed, errno=%d %m", errno);

	int fds[2];
	if (pipe(fds) < 0)
	{
		::close(_epfd);
		throw XERROR_FMT(XError, "pipe() failed, errno=%d %m", errno);
	}
	_inter_rfd = fds[0];
	_inter_wfd = fds[1];

	_tm_ref_msec = 0;
	_fd_ref_id = 0;
	_ep_events.resize(256);
	_fd2hr.resize(256);

	struct epoll_event ep;
	ep.events = EPOLLIN;
	ep.data.fd = _inter_rfd;

	xnet_set_nonblock(_inter_rfd);
	xnet_set_nonblock(_inter_wfd);
	if (epoll_ctl(_epfd, EPOLL_CTL_ADD, _inter_rfd, &ep) < 0)
	{
		::close(_epfd);
		::close(_inter_rfd);
		::close(_inter_wfd);
		throw XERROR_FMT(XError, "epoll_ctl() failed, errno=%d %m", errno);
	}

	_tm_lastid = 0;
	_fd_lastid = 0;
	_fcount = 0;
	_tcount = 0;
	_idle_mono_msec = 0;
	_leader = NON_THREAD;
	_timeout = MAX_TIMEOUT;
	xatomic_set(&_num_idle, 0);
	_num_thread = 0;
	_min_thread = 1;
	_max_thread = 1;
	_started = false;
	_canceled = false;
	_spawning = false;
	pthread_attr_init(&_attr);
	pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);
}

static void xref_decrease(FdHandler *hr)
{
	hr->xref_dec();
}

void EpollDisp::clear_handlers()
{
	if (_epfd >= 0)
	{
		::close(_epfd);
		_epfd = -1;

		::close(_inter_rfd);
		_inter_rfd = -1;

		::close(_inter_wfd);
		_inter_wfd = -1;

		std::vector<FdMan> fms;
		std::vector<TaskMan> tms;

		_fd2hr.clear();
		_fheap.take(fms);
		_theap.take(tms);

		_fqueue.foreach(xref_decrease);
		_fqueue = FdQueue();

		for (size_t i = 0; i < fms.size(); ++i)
		{
			FdMan fm = fms[i];
			fm.hr->xref_dec();
		}

		for (size_t i = 0; i < tms.size(); ++i)
		{
			TaskMan tm = tms[i];
			tm.hr->xref_dec();
		}

		pthread_attr_destroy(&_attr);
	}
}

EpollDisp::~EpollDisp()
{
	clear_handlers();
}

inline void EpollDisp::_fd_added(FdMan& fm, int fd)
{
	if (fd >= (int)_fd2hr.size())
		_fd2hr.resize(fd + 1);

	assert(_fd2hr[fd] == NULL);
	_fd2hr[fd] = fm.hr;

	fm.turnOn(_fqueue, fd);
	++_fcount;
}

inline void EpollDisp::_fd_removed(FdMan& fm, int mark)
{
	int fd = fm.efd();

	assert(_fd2hr[fd] == fm.hr);
	_fd2hr[fd] = NULL;

	fm.turnOff(_fqueue, mark);
	--_fcount;
}

bool EpollDisp::_do_add_fd(FdMan& fm, int fd, int events, struct epoll_event& ep)
{
	if (fm.efd() == INT_MIN)
		xnet_set_nonblock(fd);

	if (fd < (int)_fd2hr.size() && _fd2hr[fd])
	{
		FdMan oldfm(_fd2hr[fd]);
		assert(oldfm.efd() == fd);
		int idx = oldfm.index();

		epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
		_fd_removed(oldfm, INT_MIN);
		oldfm.decRef();

		if (idx >= 0 && idx < (int)_fheap.size() && _fheap[idx].hr == oldfm.hr)
		{
			_fheap.erase(idx);
			fm.setIndex(-1);
			oldfm.decRef();
		}
	}

	if (epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ep) < 0)
		return false;
	_fd_added(fm, fd);
	fm.setEventsAsk(events);
	fm.incRef();
	return true;
}

bool EpollDisp::addFd(FdHandler* handler, int fd, int events)
{
	if (!handler || fd < 0)
		return false;

	events &= READ_EVENT | WRITE_EVENT | EDGE_TRIGGER | ONE_SHOT;
	struct epoll_event ep;
	ep.data.fd = fd;
	ep.events = ((events & READ_EVENT) ? (EPOLLIN | EPOLLPRI) : 0)
		| ((events & WRITE_EVENT) ? EPOLLOUT : 0);
	if (ep.events == 0)
		return false;
	if (events & EDGE_TRIGGER)
		ep.events |= EPOLLET;
	if (events & ONE_SHOT)
		ep.events |= EPOLLONESHOT;

	FdMan fm(handler);
	{
		Lock lock(*this);
		if (_canceled || fm.efd() >= 0 || fm.index() >= 0)
			return false;

		if (!_do_add_fd(fm, fd, events, ep))
			return false;
	}

	return true;
}

bool EpollDisp::replaceFd(FdHandler* handler, int fd, int events)
{
	if (!handler || fd < 0)
		return false;

	events &= READ_EVENT | WRITE_EVENT | EDGE_TRIGGER | ONE_SHOT;
	struct epoll_event ep;
	ep.data.fd = fd;
	ep.events = ((events & READ_EVENT) ? (EPOLLIN | EPOLLPRI) : 0)
		| ((events & WRITE_EVENT) ? EPOLLOUT : 0);
	if (ep.events == 0)
		return false;
	if (events & EDGE_TRIGGER)
		ep.events |= EPOLLET;
	if (events & ONE_SHOT)
		ep.events |= EPOLLONESHOT;

	FdMan fm(handler);
	{
		Lock lock(*this);
		if (_canceled)
			return false;

		int efd = fm.efd();
		if (efd >= 0)	/* modify */
		{
			if (efd != fd || fd >= (int)_fd2hr.size() || _fd2hr[fd] != handler)
				return false;

			if (fm.eventsAsk() != events || (events & ONE_SHOT))
			{
				if (epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ep) < 0)
					return false;
				fm.setEventsAsk(events);
			}
		}
		else	/* add */
		{
			if (!_do_add_fd(fm, fd, events, ep))
				return false;
		}
	}
	return true;
}

bool EpollDisp::removeFd(FdHandler* handler)
{
	if (!handler)
		return false;

	FdMan fm(handler);
	{
		Lock lock(*this);
		int fd = fm.efd();
		int idx = fm.index();
		if (fd < 0 && idx < 0)
			return false;

		if (fd >= 0 && fd < (int)_fd2hr.size() && _fd2hr[fd] == handler)
		{
			epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
			_fd_removed(fm, INT_MIN);
			fm.decRef();
		}
		if (idx >= 0 && idx < (int)_fheap.size() && _fheap[idx].hr == handler)
		{
			_fheap.erase(idx);
			fm.setIndex(-1);
			fm.decRef();
		}
	}

	return true;
}

bool EpollDisp::readyFd(FdHandler* handler, int events)
{
	if (!handler)
		return false;

	FdMan fm(handler);
	bool interrupt = false;
	{
		Lock lock(*this);
		if (_canceled)
			return false;

		if (fm.index() >= 0)
		{
			fm.combineEventsGet(events);
		}
		else
		{
			if (_timeout)
				interrupt = true;

			fm.setEventsGet(events);

			fm.setId(++_fd_lastid);
			fm.setIndex(_fheap.size());
			_fheap.push(fm);
			fm.incRef();
		}
	}

	if (interrupt)
		write(_inter_wfd, "I", 1);
	return true;
}

bool EpollDisp::addTask(TaskHandler* handler, int timeout)
{
	if (!handler || timeout < 0)
		return false;

	int64_t msec = msecMonotonic() + timeout;
	TaskMan tm(handler);
	bool interrupt = false;
	{
		Lock lock(*this);
		if (_canceled || tm.index() >= 0)
			return false;

		if (_timeout)
		{
			if (_theap.empty() || _theap.top().msec() > tm.msec())
				interrupt = true;
		}

		tm.setMsec(msec);
		tm.setId(++_tm_lastid);
		tm.setIndex(_theap.size());
		_theap.push(tm);
		++_tcount;
		handler->xref_inc();
	}

	if (interrupt)
		write(_inter_wfd, "I", 1);
	return true;
}

bool EpollDisp::replaceTask(TaskHandler* handler, int timeout)
{
	if (!handler || timeout < 0)
		return false;

	int64_t msec = msecMonotonic() + timeout;
	TaskMan tm(handler);
	bool interrupt = false;
	{
		Lock lock(*this);
		if (_canceled)
			return false;

		if (_timeout)
		{
			if (_theap.empty() || _theap.top().msec() > msec)
				interrupt = true;
		}

		int idx = tm.index();
		if (idx >= 0)	/* modify */
		{
			if (idx >= (int)_theap.size() || _theap[idx].hr != handler)
				return false;

			if (tm.msec() == msec)
				return true;
			tm.setMsec(msec);
			tm.setId(++_tm_lastid);
			_theap.fix(idx);
		}
		else	/* add */
		{
			tm.setMsec(msec);
			tm.setId(++_tm_lastid);
			tm.setIndex(_theap.size());
			_theap.push(tm);
			++_tcount;
			handler->xref_inc();
		}
	}

	if (interrupt)
		write(_inter_wfd, "I", 1);
	return true;
}

bool EpollDisp::replaceTaskLaterThan(TaskHandler* handler, int timeout)
{
	if (!handler || timeout < 0)
		return false;

	int64_t msec = msecMonotonic() + timeout;
	TaskMan tm(handler);
	bool interrupt = false;
	{
		Lock lock(*this);
		if (_canceled)
			return false;

		if (_timeout)
		{
			if (_theap.empty() || _theap.top().msec() > msec)
				interrupt = true;
		}

		int idx = tm.index();
		if (idx >= 0)	/* modify */
		{
			if (idx >= (int)_theap.size() || _theap[idx].hr != handler)
				return false;

			if (tm.msec() <= msec)
				return true;
			tm.setMsec(msec);
			tm.setId(++_tm_lastid);
			_theap.fix(idx);
		}
		else	/* add */
		{
			tm.setMsec(msec);
			tm.setId(++_tm_lastid);
			tm.setIndex(_theap.size());
			_theap.push(tm);
			++_tcount;
			handler->xref_inc();
		}
	}

	if (interrupt)
		write(_inter_wfd, "I", 1);
	return true;
}

bool EpollDisp::removeTask(TaskHandler* handler)
{
	if (!handler)
		return false;

	TaskMan tm(handler);
	{
		Lock lock(*this);
		int idx = tm.index();
		if (idx < 0)
			return false;

		if (idx < (int)_theap.size() && _theap[idx].hr == handler)
		{
			_theap.erase(idx);
			tm.setIndex(-1);
			--_tcount;
			handler->xref_dec();
		}
	}

	return true;
}

bool EpollDisp::addSignal(SignalHandler* handler, int sig)
{
	if (sig <= 0)
		return false;

	// TODO
	return true;
}

bool EpollDisp::removeSignal(SignalHandler* handler)
{
	// TODO
	return true;
}

EventMan* EpollDisp::nextEvent()
{
again:
	{
		int fd_timeout;
		int tm_timeout;
		int th_timeout;

		Lock lock(*this);
		if (_canceled)
			return NULL;

		if (_num_thread > _min_thread)
		{
			th_timeout = IDLE_INTERVAL;
			int idle = xatomic_get(&_num_idle);
			if (idle > 1)
			{
				int64_t now = msecMonotonic();
				if (!_idle_mono_msec)
					_idle_mono_msec = now;
				else if (_idle_mono_msec <= now - IDLE_INTERVAL)
				{
					if (idle > IDLE_OVERMUCH)
						_idle_mono_msec = now - IDLE_INTERVAL + IDLE_FASTREAP;
					else
						_idle_mono_msec = now;
					return NULL;
				}
				else
				{
					th_timeout = _idle_mono_msec + IDLE_INTERVAL - now;
					if (th_timeout < 1000)
						th_timeout = 1000;
				}
			}
			else
				_idle_mono_msec = 0;
		}
		else
			th_timeout = MAX_TIMEOUT;

		if (!_fheap.empty())
		{
			if (!_fd_ref_id)
				_fd_ref_id = _fd_lastid;

			if (_fheap.top().id() <= _fd_ref_id)
			{
				FdMan fm = _fheap.pop();
				EventMan* evm = EventMan::create(fm.hr);
				fm.setIndex(-1);
				return evm;
			}

			fd_timeout = 0;
		}
		else
			fd_timeout = MAX_TIMEOUT;

		if (!_theap.empty())
		{
			if (!_tm_ref_msec)
				_tm_ref_msec = msecMonotonic();

			if (_theap.top().msec() <= _tm_ref_msec)
			{
				TaskMan tm = _theap.pop();
				EventMan* evm = EventMan::create(tm.hr);
				tm.setIndex(-1);
				--_tcount;
				return evm;
			}

			int64_t diff = _theap.top().msec() - msecMonotonic();
			tm_timeout = (diff < 0) ? 0 : (diff < MAX_TIMEOUT) ? diff : MAX_TIMEOUT;
		}
		else
			tm_timeout = MAX_TIMEOUT;

		if (_fcount >= _ep_events.size())
			_ep_events.resize(_fcount + 1);

		_timeout = fd_timeout < tm_timeout ? fd_timeout : tm_timeout;
		if (_timeout > th_timeout)
			_timeout = th_timeout;
	}

	xlog(XLOG_DEBUG, "epoll_wait(), fcount=%zu, tcount=%zu, timeout=%d", _fcount, _tcount, _timeout);
	int n = epoll_wait(_epfd, &_ep_events[0], _ep_events.size(), _timeout);
	_tm_ref_msec = 0;
	_fd_ref_id = 0;
	_timeout = 0;

	if (n > 0)
	{
		for (int i = 0; i < n; ++i)
		{
			int fd = _ep_events[i].data.fd;
			if (fd == _inter_rfd)
			{
				char buf[256];
				while (read(_inter_rfd,  buf, sizeof(buf)) > 0)
				{
					continue;
				}
				continue;
			}

			int ev = _ep_events[i].events;
			int events = ((ev & (EPOLLIN | EPOLLPRI)) ? READ_EVENT : 0)
				| ((ev & (EPOLLOUT)) ? WRITE_EVENT : 0)
				| ((ev & (EPOLLHUP | EPOLLERR)) ? CLOSE_EVENT : 0);

			Lock lock(*this);
			if ((size_t)fd >= _fd2hr.size() || !_fd2hr[fd])
			{
				/* Already removed. */
				continue;
			}

			FdMan fm(_fd2hr[fd]);
			bool combine = false;
			int special = fm.eventsAsk() & (EDGE_TRIGGER | ONE_SHOT);
			if (special)
			{
				events |= special;
				if (fm.index() >= 0)
				{
					combine = true;
					fm.combineEventsGet(events);
				}
				else
					fm.incRef();
			}
			else
			{
				epoll_ctl(_epfd, EPOLL_CTL_DEL, fm.efd(), NULL);
				_fd_removed(fm, -1);
				if (fm.index() >= 0)
				{
					combine = true;
					fm.combineEventsGet(events);
					fm.decRef();
				}
			}
			
			if (!combine)
			{
				fm.setEventsGet(events);
				fm.setId(++_fd_lastid);
				fm.setIndex(_fheap.size());
				_fheap.push(fm);
			}
		}
		goto again;
	}
	else if (n == 0)
	{
		goto again;
	}
	else
	{
		if (errno == EINTR)
			goto again;

		xlog(XLOG_NOTICE, "epoll_wait() failed, errno=%d, %m", errno);
		_canceled = true;
	}

	return NULL;
}

void EpollDisp::wait_and_work()
{
	struct timespec delay;

	delay.tv_sec = THREAD_INTERVAL / 1000;
	delay.tv_nsec = (THREAD_INTERVAL % 1000) * 1000000;

	nanosleep(&delay, NULL);

	this->work();
}

void EpollDisp::work()
{
	{
		Lock lock(*this);
		_spawning = false;
	}

	DispatcherPtr me(this);

	for (uint64_t nnn = 0; true; ++nnn)
	try 
	{
		{
			Lock lock(*this);
			while (_leader != NON_THREAD)
				_cond.wait(lock);
			_leader = pthread_self();
		}

		EventMan* evm;
		do
		{
			evm = nextEvent();
		} while (evm && !evm->can_run());

		bool spawn = false;
		{
			Lock lock(*this);
			_leader = NON_THREAD;
			_cond.signal();
			if (!evm)
				break;

			int idle = xatomic_dec_return(&_num_idle);
			if (idle <= 1)
			{
				_idle_mono_msec = 0;
				spawn = (_num_thread < _max_thread && idle <= 0 && !_spawning);
				if (spawn)
				{
					_spawning = true;
					++_num_thread;
					xatomic_inc(&_num_idle);
				}
			}
		}

		if (spawn && !new_thread(true))
		{
			Lock lock(*this);
			_spawning = false;
			--_num_thread;
			xatomic_dec(&_num_idle);
		}

		while (evm)
		{
			evm->doit(me);
			evm = evm->destroy();
		}
		xatomic_inc(&_num_idle);
	}
	catch (std::exception& ex)
	{
		XError* x = dynamic_cast<XError*>(&ex);
		xlog(XLOG_ERR, "EXCEPTION: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
	}

	{
		Lock lock(*this);
		--_num_thread;
		xatomic_dec(&_num_idle);
		if (_num_thread == 0)
		{
			_finish_cond.signal();
			clear_handlers();
		}
	}
}

void EpollDisp::cancel()
{
	{
		Lock lock(*this);
		_canceled = true;
	}
	write(_inter_wfd, "I", 1);
}

static void *fast_start_thread(void *arg)
{
	XPtr<EpollDisp> disp((EpollDisp *)arg, XPTR_READY);
	disp->work();
	return NULL;
}

static void *slow_start_thread(void *arg)
{
	XPtr<EpollDisp> disp((EpollDisp *)arg, XPTR_READY);
	disp->wait_and_work();
	return NULL;
}

/* Before calling this function, 
   _num_thread and _num_idle should be increased in advance.
 */
bool EpollDisp::new_thread(bool wait)
{
	int rc;
	pthread_t thr;

	xref_inc();
	rc = pthread_create(&thr, &_attr, wait ? slow_start_thread : fast_start_thread, this);
	if (rc != 0)
	{
		xref_dec();
		xlog(XLOG_ERR, "pthread_create()=%d", rc);
		return false;
	}
	return true;
}

void EpollDisp::start()
{
	{
		Lock lock(*this);
		if (_started || _canceled)
			return;

		_started = true;
		_num_thread = _min_thread;
		xatomic_set(&_num_idle, _num_thread);
	}

	for (int i = 0; i < _min_thread; ++i)
	{
		if (!new_thread(false))
		{
			Lock lock(*this);
			--_num_thread;
			xatomic_dec(&_num_idle);
		}
	}
}

void EpollDisp::waitForCancel()
{
	if (!_started)
		start();

	Lock lock(*this);
	while (_num_thread > 0)
		_finish_cond.wait(lock);
}

void EpollDisp::setThreadPool(size_t threadMin, size_t threadMax, size_t stackSize)
{
	Lock lock(*this);
	_min_thread = threadMin;
	_max_thread = threadMax;

	if (_min_thread < 1)
		_min_thread = 1;
	if (_max_thread < _min_thread)
		_max_thread = _min_thread;
	pthread_attr_setstacksize(&_attr, stackSize);
}

void EpollDisp::getThreadPool(size_t *threadMin, size_t *threadMax, size_t *stackSize)
{
	Lock lock(*this);
	if (threadMin)
		*threadMin = _min_thread;
	if (threadMax)
		*threadMax = _max_thread;
	if (stackSize)
		pthread_attr_getstacksize(&_attr, stackSize);
}

Dispatcher::~Dispatcher()
{
}

DispatcherPtr Dispatcher::create(const char *kind)
{
	pthread_once(&_once, initialize);

	if (kind && kind[0])
	{
		// TODO
	}
	return DispatcherPtr(new EpollDisp());
}


};	// namespace XEvent


#ifdef TEST_XEVENT

#include "net.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>

class Worker;
class Listener;
class Timer;
typedef XPtr<Worker> WorkerPtr;
typedef XPtr<Listener> ListenerPtr;
typedef XPtr<Timer> TimerPtr;

class Timer: public XEvent::TaskHandler
{
	int _timeout;
	int _level;
public:
	Timer(int timeout, int level): _timeout(timeout), _level(level) {}
	int timeout() const { return _timeout; }
	virtual void event_on_task(const XEvent::DispatcherPtr& dispatcher)
	{
		dispatcher->addTask(this, _timeout);
		++_level;
		if (_level <= 10)
			dispatcher->addTask(new Timer(_timeout, _level), _timeout);
	}
};

class Worker: public XEvent::FdHandler, public XEvent::TaskHandler
{
	int _fd;
	int _timeout;
public:
	Worker(int fd, int timeout): _fd(fd), _timeout(timeout)
	{
	}

	~Worker()
	{
		if (_fd >= 0)
			close(_fd);
		xlog(-1, "Worker::~ %d", _fd);
	}

	int timeout() const 	{ return _timeout; }

	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
	{
		if (events & XEvent::READ_EVENT)
		{
			xlog(-1, "Worker::READ_EVENT");
			char buf[1024];
			int n = read(_fd, buf, sizeof(buf));
			if (n > 0)
			{
				write(2, buf, n);
				//sleep(1);
				int r = write(_fd, buf, n);
				if (r != n)
					xlog(-1, "write()=%d failed, %m", r);
			}
			else if (n == 0)
			{
				close(_fd);
				_fd = -1;
				dispatcher->cancel();
			}
		}

		if (events & XEvent::WRITE_EVENT)
		{
			xlog(-1, "Worker::WRITE_EVENT");
		}

		if (events & XEvent::CLOSE_EVENT)
		{
			close(_fd);
			_fd = -1;
			xlog(-1, "Worker::CLOSE_EVENT");
		}

		if (_fd < 0)
			dispatcher->removeFd(this);
		else
			dispatcher->replaceFd(this, _fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
	}

	virtual void event_on_task(const XEvent::DispatcherPtr& dispatcher)
	{
		xlog(-1, "Worker::event_on_task()");
		if (_fd >= 0)                                                                                     
			dispatcher->addTask(this, _timeout);
	}
};

class Listener: virtual public XEvent::FdHandler
{
	int _fd;
public:
	Listener(int fd): _fd(fd) {}
	~Listener() { close(_fd); }

	virtual void event_on_fd(const XEvent::DispatcherPtr& dispatcher, int events)
	{
		xlog(-1, "Listener::event_on_fd(), %#x", events);
		int fd = accept(_fd, NULL, NULL);
		WorkerPtr worker = new Worker(fd, 1000);
		TimerPtr timer = new Timer(1 + random() % 1000, 0);
		dispatcher->addFd(worker, fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
		dispatcher->addTask(worker, worker->timeout());
		dispatcher->addTask(timer, timer->timeout());
		dispatcher->replaceFd(this, _fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
	}
};

int main()
{
	xlog_level = XLOG_INFO;
	XEvent::DispatcherPtr disp = XEvent::Dispatcher::create();
	int fd = xnet_tcp_listen(NULL, 8888, 256);
	if (fd < 0)
		throw XERROR_FMT(XError, "xnet_tcp_listen() failed, %m");

	ListenerPtr listener = new Listener(fd);
	disp->addFd(listener, fd, XEvent::READ_EVENT | XEvent::ONE_SHOT);
	disp->setThreadPool(1, 3, 1024*1024);
	disp->waitForCancel();
	return 0;
}


#endif
