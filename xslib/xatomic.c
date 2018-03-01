#include "xatomic.h"

#if !((defined(__linux) || defined(__FreeBSD__)) && (defined(__i386) || defined(__x86_64)))
#include <stdatomic.h>


void xatomic_add(xatomic_t *v, int i)
{
	atomic_fetch_add(&v->counter, i);
}

void xatomic_sub(xatomic_t *v, int i)
{
	atomic_fetch_sub(&v->counter, i);
}

bool xatomic_sub_and_test(xatomic_t *v, int i)
{
	return (xatomic_sub_return(v, i) == 0);
}

void xatomic_inc(xatomic_t *v)
{
	atomic_fetch_add(&v->counter, 1);
}

void xatomic_dec(xatomic_t *v)
{
	atomic_fetch_sub(&v->counter, 1);
}

bool xatomic_dec_and_test(xatomic_t *v)
{
	return (xatomic_sub_return(v, 1) == 0);
}

bool xatomic_inc_and_test(xatomic_t *v)
{
	return (xatomic_add_return(v, 1) == 0);
}

bool xatomic_add_negative(xatomic_t *v, int i)
{
	return (xatomic_add_return(v, i) < 0);
}

int xatomic_add_return(xatomic_t *v, int i)
{
	int old = atomic_fetch_add(&v->counter, i);
	return old + i;
}

int xatomic_sub_return(xatomic_t *v, int i)
{
	int old = atomic_fetch_sub(&v->counter, i);
	return old - i;
}

int xatomic_xchg(xatomic_t *v, int x)
{
	return atomic_exchange(&v->counter, x);
}

int xatomic_cmpxchg(xatomic_t *v, int old, int new_value)
{
	return atomic_compare_exchange_strong(&v->counter, &old, new_value);
}



void xatomiclong_add(xatomiclong_t *v, long i)
{
	atomic_fetch_add(&v->counter, i);
}

void xatomiclong_sub(xatomiclong_t *v, long i)
{
	atomic_fetch_sub(&v->counter, i);
}

bool xatomiclong_sub_and_test(xatomiclong_t *v, long i)
{
	return (xatomiclong_sub_return(v, i) == 0);
}

void xatomiclong_inc(xatomiclong_t *v)
{
	atomic_fetch_add(&v->counter, 1);
}

void xatomiclong_dec(xatomiclong_t *v)
{
	atomic_fetch_sub(&v->counter, 1);
}

bool xatomiclong_dec_and_test(xatomiclong_t *v)
{
	return (xatomiclong_sub_return(v, 1) == 0);
}

bool xatomiclong_inc_and_test(xatomiclong_t *v)
{
	return (xatomiclong_add_return(v, 1) == 0);
}

bool xatomiclong_add_negative(xatomiclong_t *v, long i)
{
	return (xatomiclong_add_return(v, i) < 0);
}

long xatomiclong_add_return(xatomiclong_t *v, long i)
{
	long old = atomic_fetch_add(&v->counter, i);
	return old + i;
}

long xatomiclong_sub_return(xatomiclong_t *v, long i)
{
	long old = atomic_fetch_sub(&v->counter, i);
	return old - i;
}

long xatomiclong_xchg(xatomiclong_t *v, long x)
{
	return atomic_exchange(&v->counter, x);
}

long xatomiclong_cmpxchg(xatomiclong_t *v, long old, long new_value)
{
	return atomic_compare_exchange_strong(&v->counter, &old, new_value);
}

#endif
