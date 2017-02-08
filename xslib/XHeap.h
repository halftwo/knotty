/* $Id: XHeap.h,v 1.12 2012/09/20 03:21:47 jiagui Exp $ */
#ifndef XHeap_h_
#define XHeap_h_

#include "XError.h"
#include <stddef.h>	/* for size_t */
#include <vector>
#include <algorithm>

template<typename T>
struct XHeapDefaultSwap
{
	void operator()(T& t1, T& t2)
	{
		std::swap(t1, t2);
	}
};

template<typename T, typename LessFunctor = std::less<T>, typename SwapFunctor = XHeapDefaultSwap<T> > 
class XHeap
{
public:
	XHeap(size_t max_num = 0)
		: _max_num(max_num ? max_num : -1)
	{
		_vec.reserve(_max_num < 1024 ? _max_num : 1024);
	}

	XHeap(const std::vector<T>& vec): _vec(vec)
	{
		size_t n = _vec.size();
		if (n > 1)
		{
			for (size_t k = n / 2; k > 0; )
			{
				--k;
				_fixdown(&_vec[0], k, n);
			}
		}
	}

	~XHeap()
	{
	}

	bool empty() const
	{
		return _vec.empty();
	}

	size_t size() const
	{
		return _vec.size();
	}

	bool push(const T& t)
	{
		size_t n = _vec.size();
		if (n < _max_num)
		{
			_vec.push_back(t);
			if (n > 0)
				_fixup(&_vec[0], n);
			return true;
		}
		else if (_less(t, _vec[0]))
		{
			_vec[0] = t;
			_fixdown(&_vec[0], 0, n);
			return true;
		}
		return false;
	}

	T& top()
	{
		if (_vec.size() == 0)
			throw XERROR_MSG(XLogicError, "no element in the XHeap");
		return _vec[0];
	}

	T pop()
	{
		size_t n = _vec.size();
		if (n == 0)
			throw XERROR_MSG(XLogicError, "no element in the XHeap");

		T t = _vec[0];
		--n;
		if (n > 0)
		{
			_swap(_vec[0], _vec[n]);
			_vec.pop_back();
			_fixdown(&_vec[0], 0, n);
		}
		else
			_vec.pop_back();
		return t;
	}

	T& operator[](size_t k)
	{
		return _vec[k];
	}

	T& at(size_t k)
	{
		if (k >= _vec.size())
			throw XERROR(XOutRangeError);
		return _vec[k];
	}

	void erase(size_t k)
	{
		size_t n = _vec.size();
		if (k >= n)
			throw XERROR(XOutRangeError);

		--n;
		if (k < n)
		{
			_swap(_vec[k], _vec[n]);
			_vec.pop_back();
			fix(k);
		}
		else
			_vec.pop_back();
	}

	void fix(size_t k)
	{
		size_t n = _vec.size();
		if (k >= n)
			throw XERROR(XOutRangeError);

		if (k > 0)
			_fixup(&_vec[0], k);

		if (k + 1 < n)
			_fixdown(&_vec[0], k, n);
	}

	void sort(std::vector<T>& result) const
	{
		result = _vec;
		size_t n = result.size();
		while (n > 1)
		{
			--n;
			_swap(result[0], result[n]);
			_fixdown(&result[0], 0, n);
		}
	}

	void take(std::vector<T>& result)
	{
		_vec.swap(result);
		_vec.clear();
	}

	void sort_and_take(std::vector<T>& result)
	{
		_vec.swap(result);
		_vec.clear();
		size_t n = result.size();
		while (n > 1)
		{
			--n;
			_swap(result[0], result[n]);
			_fixdown(&result[0], 0, n);
		}
	}

private:
	void _fixup(T* h, size_t k)
	{
		for (size_t i; k > 0 && (i = (k - 1) / 2, _less(h[i], h[k])); k = i)
		{
			_swap(h[i], h[k]);
		}
	}

	void _fixdown(T* h, size_t k, size_t num)
	{
		for (size_t i; i = 2 * k + 1, i < num; k = i)
		{
			if (i + 1 < num && _less(h[i], h[i + 1]))
				++i;

			if (_less(h[k], h[i]))
				_swap(h[k], h[i]);
			else
				break;
		}
	}

	std::vector<T> _vec;
	size_t _max_num;
	LessFunctor _less;
	SwapFunctor _swap;
};


#endif
