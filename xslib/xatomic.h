/* $Id: xatomic.h,v 1.8 2012/09/20 03:21:47 jiagui Exp $ */
/*
 * Nearly all the content of this file is from "asm/atomic.h".
 * See "asm/atomic.h" for the details.
 */
#ifndef XATOMIC_H
#define XATOMIC_H

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

#define xatomic_inc_not_zero(v) xatomic_add_unless((v), 1, 0)
#define xatomic_inc_return(v)  	(xatomic_add_return(v, 1))
#define xatomic_dec_return(v)  	(xatomic_sub_return(v, 1))



typedef struct xatomiclong_t xatomiclong_t;

struct xatomiclong_t
{
	volatile long counter; 

#ifdef __cplusplus
	xatomiclong_t()
	{
		this->counter = 0;
	}

	xatomiclong_t(long x)
	{
		this->counter = x;
	}
#endif /* __cplusplus */
};

#define XATOMICLONG_INIT(i)		{ (i) }

#define xatomiclong_read(v)		((v)->counter)
#define xatomiclong_get(v)		((v)->counter)
#define xatomiclong_set(v, i)		(((v)->counter) = (i))

#define xatomiclong_inc_not_zero(v) 	xatomiclong_add_unless((v), 1, 0)
#define xatomiclong_inc_return(v)  	(xatomiclong_add_return(v, 1))
#define xatomiclong_dec_return(v)  	(xatomiclong_sub_return(v, 1))
 

#if !((defined(__linux) || defined(__FreeBSD__)) && (defined(__i386) || defined(__x86_64)))

#ifdef __cplusplus
extern "C" {
#endif

void xatomic_add(xatomic_t *v, int i);
void xatomic_sub(xatomic_t *v, int i);
bool xatomic_sub_and_test(xatomic_t *v, int i);
void xatomic_inc(xatomic_t *v);
void xatomic_dec(xatomic_t *v);
bool xatomic_dec_and_test(xatomic_t *v);
bool xatomic_inc_and_test(xatomic_t *v);
bool xatomic_add_negative(xatomic_t *v, int i);
int xatomic_add_return(xatomic_t *v, int i);
int xatomic_sub_return(xatomic_t *v, int i);
int xatomic_xchg(xatomic_t *v, int x);
int xatomic_cmpxchg(xatomic_t *v, int old, int new_value);


void xatomiclong_add(xatomiclong_t *v, long i);
void xatomiclong_sub(xatomiclong_t *v, long i);
bool xatomiclong_sub_and_test(xatomiclong_t *v, long i);
void xatomiclong_inc(xatomiclong_t *v);
void xatomiclong_dec(xatomiclong_t *v);
bool xatomiclong_dec_and_test(xatomiclong_t *v);
bool xatomiclong_inc_and_test(xatomiclong_t *v);
bool xatomiclong_add_negative(xatomiclong_t *v, long i);
long xatomiclong_add_return(xatomiclong_t *v, long i);
long xatomiclong_sub_return(xatomiclong_t *v, long i);
long xatomiclong_xchg(xatomiclong_t *v, long x);
long xatomiclong_cmpxchg(xatomiclong_t *v, long old, long new_value);

#ifdef __cplusplus
}
#endif

#else /* (__linux || __FreeBSD__) && (__i386 || __x86_64) */

#define LOCK_PREFIX	"lock; "

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


#if defined(__x86_64)


static inline void xatomiclong_add(xatomiclong_t *v, long i)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addq %1,%0"
		:"+m" (v->counter)
		:"ir" (i));
}

static inline void xatomiclong_sub(xatomiclong_t *v, long i)
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
static inline bool xatomiclong_sub_and_test(xatomiclong_t *v, long i)
{
	unsigned char c;

	__asm__ __volatile__(
		LOCK_PREFIX "subq %2,%0; sete %1"
		:"+m" (v->counter), "=qm" (c)
		:"ir" (i) : "memory");
	return c;
}

static inline void xatomiclong_inc(xatomiclong_t *v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "incq %0"
		:"+m" (v->counter));
}

static inline void xatomiclong_dec(xatomiclong_t *v)
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
static inline bool xatomiclong_dec_and_test(xatomiclong_t *v)
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
static inline bool xatomiclong_inc_and_test(xatomiclong_t *v)
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
static inline bool xatomiclong_add_negative(xatomiclong_t *v, long i)
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
static inline long xatomiclong_add_return(xatomiclong_t *v, long i)
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

static inline long xatomiclong_sub_return(xatomiclong_t *v, long i)
{
	return xatomiclong_add_return(v, -i);
}


#define __xg64(x) ((volatile long *)(x))

static inline long xatomiclong_xchg(xatomiclong_t *v, long x)
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
static inline long xatomiclong_cmpxchg(xatomiclong_t *v, long old, long new_value)
{
	long prev;
	__asm__ __volatile__(LOCK_PREFIX "cmpxchgq %1,%2"
			     : "=a"(prev)
			     : "r"(new_value), "m"(*__xg64(&v->counter)), "0"(old)
			     : "memory");
	return prev;
}

#undef LOCK_PREFIX

#endif /* __x86_64 */

#endif /* (__linux || __FreeBSD__) && (__i386 || __x86_64) */


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

static inline bool xatomiclong_add_unless(xatomiclong_t *v, long a, long u)
{
	long c, old;
	c = xatomiclong_read(v);
	while (1)
	{
		if (c == u)	
			break;
		old = xatomiclong_cmpxchg(v, c, c + a);
		if (old == c)
			break;
		c = old;
	}
	return c != u;
}


#endif
