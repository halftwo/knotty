/* $Id: XRefCount.h,v 1.12 2015/04/23 06:55:38 gremlin Exp $ */
/*
   The reference counting class and the corresponding 
   smart pointer class template.

   Author: XIONG Jiagui
   Date: 2007-01-20
 */
#ifndef XRefCount_h_
#define XRefCount_h_

#include "xatomic.h"
#include "XError.h"
#include <typeinfo>
#include <assert.h>
#include <stdlib.h>


class XRefCount
{
public:
	XRefCount(const XRefCount& r)		: _xref_count(0) {}

	XRefCount& operator=(const XRefCount& r) { return *this; }

	long xref_count() const 		{ return xatomic_get(&_xref_count); }

	void xref_inc()
	{
		xatomic_inc(&_xref_count); 
	}

	void xref_dec()
	{
		assert(xatomic_get(&_xref_count) > 0);
		if (xatomic_dec_and_test(&_xref_count))
			xref_destroy();
	}

	bool xref_dec_only()		// Return true if _xref_count is 0 after decrease.
	{
		assert(xatomic_get(&_xref_count) > 0);
		return xatomic_dec_and_test(&_xref_count);
	}

protected:
	XRefCount(): _xref_count(0) 		{}
	virtual ~XRefCount() 			{}
	virtual void xref_destroy()		{ delete this; }

	xatomic_t _xref_count;
};

inline void increment_xref_count(XRefCount *obj)
{
	obj->xref_inc();
}

inline void decrement_xref_count(XRefCount *obj)
{
	obj->xref_dec();
}


extern bool XPtr_throw_on_null;

extern const class XPtr_Ready_t{} XPTR_READY;

template<typename T>
class XPtr 
{
public:
	XPtr()					: _p((T*)0) 	{}

	template<class Y>
	XPtr(Y* p)				: _p(p) 	{ if (_p) _p->xref_inc(); }

	template<class Y>
	XPtr(Y* p, const XPtr_Ready_t&)		: _p(p) 	{}

	XPtr(const XPtr& r)			: _p(r._p) 	{ if (_p) _p->xref_inc(); }

	template<typename Y> 
	XPtr(const XPtr<Y>& r)			: _p(r.get()) 	{ if (_p) _p->xref_inc(); }

	XPtr& operator=(const XPtr& r)		{ reset(r._p); return *this; }

	template<typename Y> 
	XPtr& operator=(const XPtr<Y>& r)	{ reset(r.get()); return *this; }

	~XPtr() 				{ if (_p) _p->xref_dec(); }

	T* operator->() const 			{ if (!_p) null_pointer(); return _p; }
	T& operator*() const 			{ if (!_p) null_pointer(); return *_p; }
	T* get() const 				{ return _p; }

	typedef T* XPtr<T>::*my_pointer_bool;
	operator my_pointer_bool() const	{ return _p ? &XPtr<T>::_p : 0; }

	bool unique() const 			{ return _p && _p->xref_count() == 1; }
	long use_count() const 			{ return _p ? _p->xref_count() : 0; }

	void reset();

	template<typename Y> 
	void reset(Y *p);

	// If non-null pointer returned, the caller should call T::xref_destroy().
	// Only use this function when T::xref_destroy() is heavy.
	T* reset_and_return_zombie(T *p);

	void swap(XPtr& r) 			{ T *tmp = r._p; r._p = _p; _p = tmp; }


	// Compare the object instead of the pointer.

	template<typename Y> bool eq(const XPtr<Y>& r) const;
	template<typename Y> bool ne(const XPtr<Y>& r) const;
	template<typename Y> bool gt(const XPtr<Y>& r) const;
	template<typename Y> bool ge(const XPtr<Y>& r) const;
	template<typename Y> bool lt(const XPtr<Y>& r) const;
	template<typename Y> bool le(const XPtr<Y>& r) const;


	// Dynamic cast from base class to derived class

	template<typename Y>
	static XPtr cast(Y* p) 			{ return XPtr(dynamic_cast<T*>(p)); }

	template<typename Y>
	static XPtr cast(const XPtr<Y>& r) 	{ return XPtr(dynamic_cast<T*>(r.get())); }

private:
	void null_pointer() const
	{
		if (XPtr_throw_on_null)
			throw XNullPointerError(__FILE__, __LINE__, 0, typeid(_p).name());
		abort();
	}

	T* _p;
};


// Implementations


template<typename T>
inline void XPtr<T>::reset()
{
	if (_p)
	{
		T *ptr = _p;
		_p = 0;
		ptr->xref_dec();
	}
}

template<typename T> template<typename Y>
inline void XPtr<T>::reset(Y *p)
{
	if (_p != p)
	{
		if (p)
			p->xref_inc();

		T *ptr = _p;
		_p = p;

		if (ptr)
			ptr->xref_dec();
	}
}

template<typename T>
inline T* XPtr<T>::reset_and_return_zombie(T *p)
{
	if (_p != p)
	{
		if (p)
			p->xref_inc();

		T *ptr = _p;
		_p = p;

		if (ptr && ptr->xref_dec_only())
			return ptr;
	}
	return (T*)0;
}


template<typename T> template<typename Y>
inline bool XPtr<T>::eq(const XPtr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p == *q;
	else
		return !_p && !q;
}

template<typename T> template<typename Y>
inline bool XPtr<T>::ne(const XPtr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p != *q;
	else
		return _p || q;
}

template<typename T> template<typename Y>
inline bool XPtr<T>::gt(const XPtr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p > *q;
	else
		return _p && !q;
}

template<typename T> template<typename Y>
inline bool XPtr<T>::ge(const XPtr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p >= *q;
	else
		return !q;
}

template<typename T> template<typename Y>
inline bool XPtr<T>::lt(const XPtr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p < *q;
	else
		return !_p && q;
}

template<typename T> template<typename Y>
inline bool XPtr<T>::le(const XPtr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p <= *q;
	else
		return !_p;
}


template<typename T>
inline void swap(XPtr<T>& a, const XPtr<T>& b)
{
	a.swap(b);
}

template<typename T, typename Y>
inline bool operator==(const XPtr<T>& lhs, const XPtr<Y>& rhs)
{
	return lhs.get() == rhs.get();
}

template<typename T, typename Y>
inline bool operator!=(const XPtr<T>& lhs, const XPtr<Y>& rhs)
{
	return lhs.get() != rhs.get();
}

template<typename T, typename Y>
inline bool operator<(const XPtr<T>& lhs, const XPtr<Y>& rhs)
{
	return lhs.get() < rhs.get();
}

template<typename T, typename Y>
inline bool operator<=(const XPtr<T>& lhs, const XPtr<Y>& rhs)
{
	return lhs.get() <= rhs.get();
}

template<typename T, typename Y>
inline bool operator>(const XPtr<T>& lhs, const XPtr<Y>& rhs)
{
	return lhs.get() > rhs.get();
}

template<typename T, typename Y>
inline bool operator>=(const XPtr<T>& lhs, const XPtr<Y>& rhs)
{
	return lhs.get() >= rhs.get();
}


typedef XPtr<XRefCount> XRefCountPtr;


#endif

