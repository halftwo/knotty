/* $Id: xptr.h,v 1.29 2012/09/20 03:21:47 jiagui Exp $ */
/*
   A kind of smart pointer support reference counting.

   Author: XIONG Jiagui
   Date: 2006-05-27
 */
#ifndef XPTR_H_
#define XPTR_H_

#include "xatomic.h"
#include "XError.h"
#include <algorithm>
#include <typeinfo>
#include <assert.h>
#include <stdlib.h>


extern bool xptr_throw_on_null;


class xptr_refcount
{
public:
	static void *operator new(size_t size);
	static void operator delete(void *ptr);

	xptr_refcount()			: _use(1), _weak(1) {}

	long use_count() const		{ return xatomic_read(&_use); }

	void strongref_inc()
	{
		xatomic_inc(&_use);
	}

	bool strongref_test_inc()
	{
		return xatomic_inc_not_zero(&_use);
	}

	long strongref_dec()
	{
		assert(xatomic_get(&_use) > 0);
		long x = xatomic_dec_return(&_use);
		if (x == 0)
			weakref_dec();
		return x;
	}

	void weakref_inc()
	{
		xatomic_inc(&_weak);
	}

	void weakref_dec()
	{
		assert(xatomic_get(&_weak) > 0);
		if (xatomic_dec_and_test(&_weak))
			delete this;
	}

private:
	xatomic_t _use;
	xatomic_t _weak;
};


template<typename T> class weak_xptr;


template<typename T>
class xptr 
{
public:
	xptr()					: _rc(0), _p(0) {}

	template<typename Y>
	explicit xptr(Y *p);

	xptr(const xptr& r)			: _rc(r._rc), _p(r._p) { if (_p) _rc->strongref_inc(); }

	template<typename Y>
	xptr(const xptr<Y>& r)			: _rc(r._rc), _p(r._p) { if (_p) _rc->strongref_inc(); }

	template<typename Y>
	explicit xptr(const weak_xptr<Y>& r);

	xptr& operator=(const xptr& r);

	template<typename Y>
	xptr& operator=(const xptr<Y>& r);

	~xptr() 				{ if (_rc && _rc->strongref_dec() == 0) delete _p; }

	T* operator->() const 			{ if (!_p) null_pointer(); return _p; }
	T& operator*() const 			{ if (!_p) null_pointer(); return *_p; }
	T* get() const 				{ return _p; }

	typedef T* xptr<T>::*my_pointer_bool;
	operator my_pointer_bool() const	{ return _p ? &xptr<T>::_p : 0; }

	bool unique() const 			{ return _rc && _rc->use_count() == 1; }
	long use_count() const 			{ return _rc ? _rc->use_count() : 0; }

	void reset();

	template <typename Y>
	void reset(Y *p);

	void swap(xptr& r) 			{ std::swap(_rc, r._rc); std::swap(_p, r._p); }


	// Compare the object instead of the pointer.

	template<typename Y> bool eq(const xptr<Y>& r) const;
	template<typename Y> bool ne(const xptr<Y>& r) const;
	template<typename Y> bool gt(const xptr<Y>& r) const;
	template<typename Y> bool ge(const xptr<Y>& r) const;
	template<typename Y> bool lt(const xptr<Y>& r) const;
	template<typename Y> bool le(const xptr<Y>& r) const;


	// Dynamic cast from base class

	template<typename Y>
	static xptr cast(Y *p) 			{ return xptr(dynamic_cast<T*>(p)); }

	template<typename Y>
	static xptr cast(const xptr<Y>& r)
	{
		xptr z;
		z._p = dynamic_cast<T*>(r._p);
		if (z._p)
		{
			r._rc->strongref_inc();
			z._rc = r._rc;
		}
		return z;
	}

private:
	template<typename Y> friend class xptr;
	template<typename Y> friend class weak_xptr;
 
	void null_pointer() const
	{
		if (xptr_throw_on_null)
			throw XNullPointerError(__FILE__, __LINE__, 0, typeid(_p).name());
		abort();
	}

	xptr_refcount *_rc;
	T *_p;
};

template<typename T>
void swap(xptr<T>& a, xptr<T>& b);

template<typename T, typename Y>
bool operator==(const xptr<T>& lhs, const xptr<Y>& rhs);

template<typename T, typename Y>
bool operator!=(const xptr<T>& lhs, const xptr<Y>& rhs);

template<typename T, typename Y>
bool operator<(const xptr<T>& lhs, const xptr<Y>& rhs);

template<typename T, typename Y>
bool operator<=(const xptr<T>& lhs, const xptr<Y>& rhs);

template<typename T, typename Y>
bool operator>(const xptr<T>& lhs, const xptr<Y>& rhs);

template<typename T, typename Y>
bool operator>=(const xptr<T>& lhs, const xptr<Y>& rhs);


template<typename T>
class weak_xptr 
{
public:
	weak_xptr()				: _rc(0), _p(0) {}

	weak_xptr(const weak_xptr& r)		: _rc(r._rc), _p(r._p) { if (_rc) _rc->weakref_inc(); }

	template<typename Y>
	weak_xptr(const weak_xptr<Y>& r)	: _rc(r._rc), _p(r._p) { if (_rc) _rc->weakref_inc(); }

	template<typename Y>
	weak_xptr(const xptr<Y>& r)		: _rc(r._rc), _p(r._p) { if (_rc) _rc->weakref_inc(); }

	weak_xptr& operator=(const weak_xptr& r);

	template<typename Y>
	weak_xptr& operator=(const weak_xptr<Y>& r);

	template<typename Y>
	weak_xptr& operator=(const xptr<Y>& r);

	~weak_xptr() 				{ if (_rc) _rc->weakref_dec(); }

	xptr<T> hold() const 			{ xptr<T> p(*this); return p; }
	bool expire() const 			{ return !_rc || _rc->use_count() == 0; }
	long use_count() const 			{ return _rc ? _rc->use_count() : 0; }

	void reset();
	void swap(weak_xptr& r) 		{ std::swap(_rc, r._rc); std::swap(_p, r._p); }


	// Dynamic cast from base class

	template<typename Y>
	static weak_xptr cast(const weak_xptr<Y>& r)
	{
		weak_xptr z;
		z._p = dynamic_cast<T*>(r._p);
		if (z._p)
		{
			if (r._rc)
				r._rc->weakref_inc();
			z._rc = r._rc;
		}
		return z;
	}

private:
	template<typename Y> friend class weak_xptr;
	template<typename Y> friend class xptr;
 
	xptr_refcount *_rc;
	T *_p;
};

template<typename T>
void swap(weak_xptr<T>& a, weak_xptr<T>& b);



// Implementations


template<typename T> template<typename Y> 
inline xptr<T>::xptr(const weak_xptr<Y>& r)
{
	if (r._rc && r._rc->strongref_test_inc())
	{
		_rc = r._rc;
		_p = r._p;
	}
	else
	{
		_rc = 0;
		_p = 0;
	}
}

template<typename T> template<typename Y> 
inline xptr<T>::xptr(Y* p)
{
	if (p)
	{
		try
		{
			_rc = new xptr_refcount(); 
			_p = p;
		}
		catch (...)
		{
			delete p;
			throw;
		}
	}
	else
	{
		_rc = 0;
		_p = 0;
	}
}

template<typename T>
inline xptr<T>& xptr<T>::operator=(const xptr& r)
{
	if (_p != r._p)
	{
		if (r._p)
			r._rc->strongref_inc();
		if (_p && _rc->strongref_dec() == 0)
			delete _p;
		_rc = r._rc;
		_p = r._p;
	}
	return *this;
}

template<typename T> template<typename Y> 
inline xptr<T>& xptr<T>::operator=(const xptr<Y>& r)
{
	if (_p != r._p)
	{
		if (r._p)
			r._rc->strongref_inc();
		if (_p && _rc->strongref_dec() == 0)
			delete _p;
		_rc = r._rc;
		_p = r._p;
	}
	return *this;
}

template<typename T>
inline void xptr<T>::reset()
{
	if (_p && _rc->strongref_dec() == 0)
		delete _p;
	_rc = 0;
	_p = 0;
}

template<typename T> template<typename Y> 
inline void xptr<T>::reset(Y *p)
{
	if (p != _p)
	{
		if (_p && _rc->strongref_dec() == 0)
			delete _p;
		
		_rc = 0;
		_p = 0;
		if (p)
		{
			try
			{
				_rc = new xptr_refcount();
				_p = p;
			}
			catch (...)
			{
				delete p;
				throw;
			}
		}
	}
}

template<typename T> template<typename Y>
inline bool xptr<T>::eq(const xptr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p == *q;
	else
		return !_p && !q;
}

template<typename T> template<typename Y>
inline bool xptr<T>::ne(const xptr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p != *q;
	else
		return _p || q;
}

template<typename T> template<typename Y>
inline bool xptr<T>::gt(const xptr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p > *q;
	else
		return _p && !q;
}

template<typename T> template<typename Y>
inline bool xptr<T>::ge(const xptr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p >= *q;
	else
		return !q;
}

template<typename T> template<typename Y>
inline bool xptr<T>::lt(const xptr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p < *q;
	else
		return !_p && q;
}

template<typename T> template<typename Y>
inline bool xptr<T>::le(const xptr<Y>& r) const
{
	Y* q = r.get();
	if (_p && q)
		return *_p <= *q;
	else
		return !_p;
}


template<typename T>
inline void swap(xptr<T>& a, xptr<T>& b)
{
	a.swap(b);
}

template<typename T, typename Y>
inline bool operator==(const xptr<T>& lhs, const xptr<Y>& rhs)
{
	return lhs.get() == rhs.get();
}

template<typename T, typename Y>
inline bool operator!=(const xptr<T>& lhs, const xptr<Y>& rhs)
{
	return lhs.get() != rhs.get();
}

template<typename T, typename Y>
inline bool operator<(const xptr<T>& lhs, const xptr<Y>& rhs)
{
	return lhs.get() < rhs.get();
}


template<typename T, typename Y>
inline bool operator<=(const xptr<T>& lhs, const xptr<Y>& rhs)
{
	return lhs.get() <= rhs.get();
}

template<typename T, typename Y>
inline bool operator>(const xptr<T>& lhs, const xptr<Y>& rhs)
{
	return lhs.get() > rhs.get();
}

template<typename T, typename Y>
inline bool operator>=(const xptr<T>& lhs, const xptr<Y>& rhs)
{
	return lhs.get() >= rhs.get();
}


template<typename T>
inline weak_xptr<T>& weak_xptr<T>::operator=(const weak_xptr& r)
{
	if (_rc != r._rc)
	{
		if (r._rc)
			r._rc->weakref_inc();
		if (_rc)
			_rc->weakref_dec();
		_rc = r._rc;
		_p = r._p;
	}
	return *this;
}

template<typename T> template<typename Y>
inline weak_xptr<T>& weak_xptr<T>::operator=(const weak_xptr<Y>& r)
{
	if (_rc != r._rc)
	{
		if (r._rc)
			r._rc->weakref_inc();
		if (_rc)
			_rc->weakref_dec();
		_rc = r._rc;
		_p = r._p;
	}
	return *this;
}

template<typename T> template<typename Y>
inline weak_xptr<T>& weak_xptr<T>::operator=(const xptr<Y>& r)
{
	if (_rc != r._rc)
	{
		if (r._rc)
			r._rc->weakref_inc();
		if (_rc)
			_rc->weakref_dec();
		_rc = r._rc;
		_p = r._p;
	}
	return *this;
}

template<typename T>
inline void weak_xptr<T>::reset()
{
	if (_rc)
		_rc->weakref_inc();
	_rc = 0;
	_p = 0;
}

template<typename T>
inline void swap(weak_xptr<T>& a, weak_xptr<T>& b)
{
	a.swap(b);
}

#endif

