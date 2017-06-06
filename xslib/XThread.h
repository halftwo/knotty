/* $Id: XThread.h,v 1.7 2014/06/06 08:46:53 gremlin Exp $ */
#ifndef XThread_h_
#define XThread_h_

#include "XRefCount.h"
#include "XLock.h"
#include "XError.h"
#include "UniquePtr.h"
#include <pthread.h>


XE_(XError,	XThreadError);


class XThread;
typedef XPtr<XThread> XThreadPtr;


class XThread: public XRefCount
{
	typedef pthread_t id_t;

	id_t _id;
	bool _joinable;

	XThread(id_t id, bool joinable);
	XThread(const XThread&);
	XThread& operator=(const XThread&);

	static XThreadPtr create_thread(void *(*routine)(void *), void *arg, bool joinable, size_t stackSize);

	template <class T>
	class ObjHelper0
	{
		T *obj;
		void (T::*mfun)();

		ObjHelper0(T *obj_, void (T::*mfun_)()) : obj(obj_), mfun(mfun_) {}

		static void* routine(void *helper)
		{
			UniquePtr<ObjHelper0> p((ObjHelper0 *)helper);
			(p->obj->*(p->mfun))();
			return NULL;
		}
	public:
		static XThreadPtr create(T& obj, void (T::*mfun)(), bool joinable, size_t stackSize)
		{
			UniquePtr<ObjHelper0> helper(new ObjHelper0(&obj, mfun));
			XThreadPtr thr = XThread::create_thread(routine, helper.get(), joinable, stackSize);
			helper.release();
			return thr;
		}
	};

	template <class T, typename P1>
	class ObjHelper1
	{
		T *obj;
		void (T::*mfun)(P1);
		const P1& p1;

		ObjHelper1(T *obj_, void (T::*mfun_)(P1), P1& p1_) : obj(obj_), mfun(mfun_), p1(p1_) {}

		static void* routine(void *helper)
		{
			UniquePtr<ObjHelper1> p((ObjHelper1 *)helper);
			(p->obj->*(p->mfun))(p->p1);
			return NULL;
		}
	public:
		static XThreadPtr create(T& obj, void (T::*mfun)(P1), P1& p1, bool joinable, size_t stackSize)
		{
			UniquePtr<ObjHelper1> helper(new ObjHelper1(&obj, mfun, p1));
			XThreadPtr thr = XThread::create_thread(routine, helper.get(), joinable, stackSize);
			helper.release();
			return thr;
		}
	};

	template <class T>
	class RCHelper0
	{
		XPtr<T> obj;
		void (T::*mfun)();

		RCHelper0(T *obj_, void (T::*mfun_)()) : obj(obj_), mfun(mfun_) {}

		static void* routine(void *helper)
		{
			UniquePtr<RCHelper0> p((RCHelper0 *)helper);
			(p->obj.get()->*(p->mfun))();
			return NULL;
		}
	public:
		static XThreadPtr create(T* obj, void (T::*mfun)(), bool joinable, size_t stackSize)
		{
			UniquePtr<RCHelper0> helper(new RCHelper0(obj, mfun));
			XThreadPtr thr = XThread::create_thread(routine, helper.get(), joinable, stackSize);
			helper.release();
			return thr;
		}
	};

	template <class T, typename P1>
	class RCHelper1
	{
		XPtr<T> obj;
		void (T::*mfun)(P1);
		const P1& p1;

		RCHelper1(T *obj_, void (T::*mfun_)(P1), P1& p1_) : obj(obj_), mfun(mfun_), p1(p1_) {}

		static void* routine(void *helper)
		{
			UniquePtr<RCHelper1> p((RCHelper1 *)helper);
			(p->obj.get()->*(p->mfun))(p->p1);
			return NULL;
		}
	public:
		static XThreadPtr create(T* obj, void (T::*mfun)(P1), P1& p1, bool joinable, size_t stackSize)
		{
			UniquePtr<RCHelper1> helper(new RCHelper1(obj, mfun, p1));
			XThreadPtr thr = XThread::create_thread(routine, helper.get(), joinable, stackSize);
			helper.release();
			return thr;
		}
	};

public:
	typedef id_t ID;

	static XThreadPtr create(void *(*routine)(void *), void *arg, 
				bool joinable = false, size_t stackSize = 0)
	{
		return create_thread(routine, arg, joinable, stackSize);
	}

	template <class T>
	static XThreadPtr create(T& obj, void (T::*mfun)(), 
				bool joinable = false, size_t stackSize = 0)
	{
		return ObjHelper0<T>::create(obj, mfun, joinable, stackSize);
	}

	template <class T>
	static XThreadPtr create(const XPtr<T>& obj, void (T::*mfun)(), 
				bool joinable = false, size_t stackSize = 0)
	{
		return RCHelper0<T>::create(obj.get(), mfun, joinable, stackSize);
	}

	template <class T>
	static XThreadPtr create(T* obj, void (T::*mfun)(), 
				bool joinable = false, size_t stackSize = 0)
	{
		return RCHelper0<T>::create(obj, mfun, joinable, stackSize);
	}

	template <class T, typename P1>
	static XThreadPtr create(T& obj, void (T::*mfun)(P1), P1& p1, 
				bool joinable = false, size_t stackSize = 0)
	{
		return ObjHelper1<T,P1>::create(obj, mfun, p1, joinable, stackSize);
	}

	template <class T, typename P1>
	static XThreadPtr create(T* obj, void (T::*mfun)(P1), P1& p1, 
				bool joinable = false, size_t stackSize = 0)
	{
		return RCHelper1<T,P1>::create(obj, mfun, p1, joinable, stackSize);
	}

	template <class T, typename P1>
	static XThreadPtr create(const XPtr<T>& obj, void (T::*mfun)(P1), P1& p1,
				bool joinable = false, size_t stackSize = 0)
	{
		return RCHelper1<T,P1>::create(obj.get(), mfun, p1, joinable, stackSize);
	}

	ID id() const		{ return _id; }
	bool joinable() const 	{ return _joinable; }

	void* join();
	void detach();
};


#endif
