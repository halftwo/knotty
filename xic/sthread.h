#ifndef sthread_h_
#define sthread_h_

#include "xslib/UniquePtr.h"
#include <st.h>


template <class T>
class SThreadObjHelper
{
public:
	static st_thread_t create(T& obj, void (T::*mfun)(), bool joinable, size_t stackSize)
	{
		UniquePtr<SThreadObjHelper> helper(new SThreadObjHelper(&obj, mfun));
		st_thread_t thr = st_thread_create(routine, helper.get(), joinable, stackSize);
		helper.release();
		return thr;
	}

private:
	static void* routine(void *helper)
	{
		SThreadObjHelper *p = (SThreadObjHelper *)helper;
		T *obj = p->obj;
		void (T::*mfun)() = p->mfun;
		delete p;

		(obj->*mfun)();
		return NULL;
	}

	SThreadObjHelper(T *obj_, void (T::*mfun_)()) 	: obj(obj_), mfun(mfun_) {}

	T *obj;
	void (T::*mfun)();
};

template <class T>
class SThreadRCHelper
{
public:
	static st_thread_t create(T* obj, void (T::*mfun)(), bool joinable, size_t stackSize)
	{
		UniquePtr<SThreadRCHelper> helper(new SThreadRCHelper(obj, mfun));
		st_thread_t thr = st_thread_create(routine, helper.get(), joinable, stackSize);
		helper.release();
		return thr;
	}

private:
	static void* routine(void *helper)
	{
		SThreadRCHelper *p = (SThreadRCHelper *)helper;
		XPtr<T> obj;
		obj.swap(p->obj);
		void (T::*mfun)() = p->mfun;
		delete p;

		(obj.get()->*mfun)();
		return NULL;
	}

	SThreadRCHelper(T *obj_, void (T::*mfun_)()) 	: obj(obj_), mfun(mfun_) {}

	XPtr<T> obj;
	void (T::*mfun)();
};


template <class T>
st_thread_t sthread_create(T& obj, void (T::*mfun)(), bool joinable = false, size_t stackSize = 0)
{
	return SThreadObjHelper<T>::create(obj, mfun, joinable, stackSize);
}

template<class T>
st_thread_t sthread_create(T* obj, void (T::*mf)(), bool joinable = false, int stackSize = 0)
{
	return SThreadRCHelper<T>::create(obj, mf, joinable, stackSize);
}

template <class T>
st_thread_t sthread_create(const XPtr<T>& obj, void (T::*mf)(), bool joinable = false, int stackSize = 0)
{
	return SThreadRCHelper<T>::create(obj.get(), mf, joinable, stackSize);
}

#endif
