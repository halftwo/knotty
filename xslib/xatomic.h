/* $Id: xatomic.h,v 1.8 2012/09/20 03:21:47 jiagui Exp $ */
/*
 * Nearly all the content of this file is from "asm/atomic.h".
 * See "asm/atomic.h" for the details.
 */
#ifndef XATOMIC_H
#define XATOMIC_H

#if (defined(__linux) || defined(__FreeBSD__)) && (defined(__i386) || defined(__x86_64))

#define LOCK_PREFIX	"lock; "


#include <stdbool.h>



typedef struct xatomic_t xatomic_t;

struct xatomic_t
{
	volatile int counter; 

#ifdef __cplusplus
	xatomic_t()
	{
		this->counter = 0;
	}

	xatomic_t(int x)
	{
		this->counter = x;
	}
#endif /* __cplusplus */
};



#define XATOMIC_INIT(i)		{ (i) }

#define xatomic_read(v)		((v)->counter)

#define xatomic_get(v)		((v)->counter)

#define xatomic_set(v, i)	(((v)->counter) = (i))


static inline void xatomic_add(xatomic_t *v, int i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addl %1,%0"
		:"+m" (v->counter)
		:"ir" (i));
}

static inline void xatomic_sub(xatomic_t *v, int i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "subl %1,%0"
		:"+m" (v->counter)
		:"ir" (i));
}

/**
 * Atomically subtracts @i from @v and 
 * returns true if the result is zero, 
 * or false for all other cases.
 */
static inline bool xatomic_sub_and_test(xatomic_t *v, int i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subl %2,%0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i) : "memory");
	return c;
}

static inline void xatomic_inc(xatomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incl %0"
		:"+m" (v->counter));
}

static inline void xatomic_dec(xatomic_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decl %0"
		:"+m" (v->counter));
}

/**
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, 
 * or false for all other cases.
 */ 
static inline bool xatomic_dec_and_test(xatomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decl %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		: : "memory");
	return c != 0;
}

/**
 * Atomically increments @v by 1
 * and returns true if the result is zero, 
 * or false for all other cases.
 */ 
static inline bool xatomic_inc_and_test(xatomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incl %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		: : "memory");
	return c != 0;
}

/**
 * Atomically adds @i to @v and returns true
 * if the result is negative,
 * or false when result is greater than or equal to zero.
 */ 
static inline bool xatomic_add_negative(xatomic_t *v, int i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addl %2,%0; sets %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i) : "memory");
	return c;
}

/**
 * Atomically adds @i to @v and returns @i + @v
 */
static inline int xatomic_add_return(xatomic_t *v, int i)
{
	int __i;
	/* Modern 486+ processor */
	__i = i;
	__asm__ __volatile__(
		LOCK_PREFIX "xaddl %0, %1"
		:"+r" (i), "+m" (v->counter)
		: : "memory");
	return i + __i;
}

static inline int xatomic_sub_return(xatomic_t *v, int i)
{
	return xatomic_add_return(v, -i);
}


#define __xg(x) ((volatile int *)(x))

static inline int xatomic_xchg(xatomic_t *v, int x)
{
	__asm__ __volatile__("xchgl %0,%1"
		:"=r" (x)
		:"m" (*__xg(&v->counter)), "0" (x)
		:"memory");
	return x;
}

/**
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
static inline int xatomic_cmpxchg(xatomic_t *v, int old, int new_value)
{
	int prev;
	__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %k1,%2"
			     : "=a"(prev)
			     : "r"(new_value), "m"(*__xg(&v->counter)), "0"(old)
			     : "memory");
	return prev;
}

/**
 * Atomically adds @a to @v, so long as @v was not already @u.
 * Returns true if @v was not @u, and false otherwise.
 */
static inline bool xatomic_add_unless(xatomic_t *v, int a, int u)
{
	int c, old;
	c = xatomic_read(v);
	while (1)
	{
		if (c == u)	
			break;
		old = xatomic_cmpxchg(v, c, c + a);
		if (old == c)
			break;
		c = old;
	}
	return c != u;
}

#define xatomic_inc_not_zero(v) xatomic_add_unless((v), 1, 0)

#define xatomic_inc_return(v)  (xatomic_add_return(v, 1))
#define xatomic_dec_return(v)  (xatomic_sub_return(v, 1))




#if defined(__x86_64)

typedef struct xatomic64_t xatomic64_t;

struct xatomic64_t
{
	volatile long counter; 

#ifdef __cplusplus
	xatomic64_t()
	{
		this->counter = 0;
	}

	xatomic64_t(long x)
	{
		this->counter = x;
	}
#endif /* __cplusplus */
};


#define XATOMIC64_INIT(i)	{ (i) }

#define xatomic64_read(v)	((v)->counter)

#define xatomic64_get(v)	((v)->counter)

#define xatomic64_set(v, i)	(((v)->counter) = (i))


static inline void xatomic64_add(xatomic64_t *v, long i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addq %1,%0"
		:"+m" (v->counter)
		:"ir" (i));
}

static inline void xatomic64_sub(xatomic64_t *v, long i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "subq %1,%0"
		:"+m" (v->counter)
		:"ir" (i));
}

/**
 * Atomically subtracts @i from @v and 
 * returns true if the result is zero, 
 * or false for all other cases.
 */
static inline bool xatomic64_sub_and_test(xatomic64_t *v, long i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subq %2,%0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i) : "memory");
	return c;
}

static inline void xatomic64_inc(xatomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incq %0"
		:"+m" (v->counter));
}

static inline void xatomic64_dec(xatomic64_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "decq %0"
		:"+m" (v->counter));
}

/**
 * Atomically decrements @v by 1 and
 * returns true if the result is 0, 
 * or false for all other cases.
 */ 
static inline bool xatomic64_dec_and_test(xatomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "decq %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		: : "memory");
	return c != 0;
}

/**
 * Atomically increments @v by 1
 * and returns true if the result is zero, 
 * or false for all other cases.
 */ 
static inline bool xatomic64_inc_and_test(xatomic64_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "incq %0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		: : "memory");
	return c != 0;
}

/**
 * Atomically adds @i to @v and returns true
 * if the result is negative,
 * or false when result is greater than or equal to zero.
 */ 
static inline bool xatomic64_add_negative(xatomic64_t *v, long i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "addq %2,%0; sets %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i) : "memory");
	return c;
}

/**
 * Atomically adds @i to @v and returns @i + @v
 */
static inline long xatomic64_add_return(xatomic64_t *v, long i)
{
	long __i;
	/* Modern 486+ processor */
	__i = i;
	__asm__ __volatile__(
		LOCK_PREFIX "xaddq %0, %1"
		:"+r" (i), "+m" (v->counter)
		: : "memory");
	return i + __i;
}

static inline long xatomic64_sub_return(xatomic64_t *v, long i)
{
	return xatomic64_add_return(v, -i);
}


#define __xg64(x) ((volatile long *)(x))

static inline long xatomic64_xchg(xatomic64_t *v, long x)
{
	__asm__ __volatile__("xchgq %0,%1"
		:"=r" (x)
		:"m" (*__xg64(&v->counter)), "0" (x)
		:"memory");
	return x;
}

/**
 * Atomic compare and exchange.  Compare OLD with MEM, if identical,
 * store NEW in MEM.  Return the initial value in MEM.  Success is
 * indicated by comparing RETURN with OLD.
 */
static inline long xatomic64_cmpxchg(xatomic64_t *v, long old, long new_value)
{
	long prev;
	__asm__ __volatile__(LOCK_PREFIX "cmpxchgq %1,%2"
			     : "=a"(prev)
			     : "r"(new_value), "m"(*__xg64(&v->counter)), "0"(old)
			     : "memory");
	return prev;
}

/**
 * Atomically adds @a to @v, so long as @v was not already @u.
 * Returns true if @v was not @u, and false otherwise.
 */
static inline bool xatomic64_add_unless(xatomic64_t *v, long a, long u)
{
	long c, old;
	c = xatomic64_read(v);
	while (1)
	{
		if (c == u)	
			break;
		old = xatomic64_cmpxchg(v, c, c + a);
		if (old == c)
			break;
		c = old;
	}
	return c != u;
}

#define xatomic64_inc_not_zero(v) 	xatomic64_add_unless((v), 1, 0)

#define xatomic64_inc_return(v)  	(xatomic64_add_return(v, 1))
#define xatomic64_dec_return(v)  	(xatomic64_sub_return(v, 1))
 

#endif /* __x86_64 */


#undef LOCK_PREFIX



#if LONG_MAX == INT_MAX


typedef xatomic_t xatomiclong_t;

#define XATOMICLONG_INIT(i)			XATOMIC_INIT(i)

#define xatomiclong_read(v)			((long)xatomic_read(v))
#define xatomiclong_get(v)			((long)xatomic_get(v))
#define xatomiclong_set(v, i)			xatomic_set(v, i)

#define xatomiclong_add(v, i)			xatomic_add(v, i)
#define xatomiclong_sub(v, i)			xatomic_sub(v, i)
#define xatomiclong_inc(v)			xatomic_inc(v)
#define xatomiclong_dec(v)			xatomic_dec(v)

#define xatomiclong_sub_and_test(v, i)		xatomic_sub_and_test(v, i)
#define xatomiclong_dec_and_test(v)		xatomic_dec_and_test(v)
#define xatomiclong_inc_and_test(v)		xatomic_inc_and_test(v)
#define xatomiclong_add_negative(v, i)		xatomic_add_negative(v, i)

static inline long xatomiclong_add_return(xatomiclong_t *v, long i)	{ return xatomic_add_return(v, i); }
static inline long xatomiclong_sub_return(xatomiclong_t *v, long i)	{ return xatomic_sub_return(v, i); }
static inline long xatomiclong_inc_return(xatomiclong_t *v)		{ return xatomic_inc_return(v); }
static inline long xatomiclong_dec_return(xatomiclong_t *v)		{ return xatomic_dec_return(v); }

static inline long xatomiclong_xchg(xatomiclong_t *v, long x)		{ return xatomic_xchg(v, x); }
static inline long xatomiclong_cmpxchg(xatomiclong_t *v, long old, long new_) { return xatomic_cmpxchg(v, old, new_); }

#define xatomiclong_add_unless(v, a, u) 	xatomic_add_unless(v, a, u)
#define xatomiclong_inc_not_zero(v)		xatomic_inc_not_zero(v)


#elif LONG_MAX > INT_MAX


typedef xatomic64_t xatomiclong_t;

#define XATOMICLONG_INIT(i)			XATOMIC64_INIT(i)

#define xatomiclong_read(v)			xatomic64_read(v)
#define xatomiclong_get(v)			xatomic64_get(v)
#define xatomiclong_set(v, i)			xatomic64_set(v, i)

#define xatomiclong_add(v, i)			xatomic64_add(v, i)
#define xatomiclong_sub(v, i)			xatomic64_sub(v, i)
#define xatomiclong_inc(v)			xatomic64_inc(v)
#define xatomiclong_dec(v)			xatomic64_dec(v)

#define xatomiclong_sub_and_test(v, i)		xatomic64_sub_and_test(v, i)
#define xatomiclong_dec_and_test(v)		xatomic64_dec_and_test(v)
#define xatomiclong_inc_and_test(v)		xatomic64_inc_and_test(v)
#define xatomiclong_add_negative(v, i)		xatomic64_add_negative(v, i)

#define xatomiclong_add_return(v, i)		xatomic64_add_return(v, i)
#define xatomiclong_sub_return(v, i)		xatomic64_sub_return(v, i)
#define xatomiclong_inc_return(v)		xatomic64_inc_return(v)
#define xatomiclong_dec_return(v)		xatomic64_dec_return(v)

#define xatomiclong_xchg(v, x)			xatomic64_xchg(v, x)
#define xatomiclong_cmpxchg(v, old, new_) 	xatomic64_cmpxchg(v, old, new_)

#define xatomiclong_add_unless(v, a, u) 	xatomic64_add_unless(v, a, u)
#define xatomiclong_inc_not_zero(v)		xatomic64_inc_not_zero(v)


#endif /* LONG_MAX > INT_MAX */



#endif

#endif
