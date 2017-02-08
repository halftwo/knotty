#ifndef ostk_allocator_h_
#define ostk_allocator_h_

#include "ostk.h"
#include <cstdlib>


template <class T>
class ostk_allocator
{
	ostk_allocator();
	void operator=(const ostk_allocator&);

public:
	ostk_t *_ostk;

public:
	typedef T			value_type;
	typedef value_type*		pointer;
	typedef const value_type* 	const_pointer;
	typedef value_type&		reference;
	typedef const value_type& 	const_reference;
	typedef std::size_t		size_type;
	typedef std::ptrdiff_t		difference_type;
	
	template <class U> 
	struct rebind
	{
		typedef ostk_allocator<U> other;
	};

	ostk_allocator(ostk_t *ostk)
		: _ostk(ostk)
	{
	}

	ostk_allocator(const ostk_allocator& r)
		: _ostk(r._ostk)
	{
	}

	template <class U> 
	ostk_allocator(const ostk_allocator<U>& r)
		: _ostk(r._ostk)
	{
	}

	~ostk_allocator()
	{
	}

	pointer address(reference x) const
	{
		return &x; 
	}

	const_pointer address(const_reference x) const
	{
		return x;
	}

	pointer allocate(size_type n, const_pointer = 0)
	{
		void* p = ostk_alloc(_ostk, n * sizeof(T));
		if (!p)
			throw std::bad_alloc();
		return static_cast<pointer>(p);
	}

	void deallocate(pointer p, size_type)
	{
		// Do nothing.
	}

	size_type max_size() const
	{ 
		return static_cast<size_type>(-1) / sizeof(T);
	}

	void construct(pointer p, const value_type& x)
	{ 
		new(p) value_type(x); 
	}

	void destroy(pointer p)
	{
		p->~value_type();
	}
};

template<> class ostk_allocator<void>
{
	typedef void		value_type;
	typedef void*		pointer;
	typedef const void* 	const_pointer;

	template <class U> 
	struct rebind
	{
		typedef ostk_allocator<U> other;
	};
};


template <class T>
inline bool operator==(const ostk_allocator<T>& l, const ostk_allocator<T>& r)
{
	return l._ostk == r._ostk;
}

template <class T>
inline bool operator!=(const ostk_allocator<T>& l, const ostk_allocator<T>& r)
{
	return l._ostk != r._ostk;
}


#endif

