/* $Id: xmem.h,v 1.6 2010/05/11 09:38:01 jiagui Exp $ */
#ifndef XMEM_H_
#define XMEM_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef	void *(*xmem_alloc_function)(void *cookie, size_t size);
typedef	void (*xmem_free_function)(void *cookie, void *ptr);


typedef struct xmem_t xmem_t;
struct xmem_t
{
	xmem_alloc_function 	alloc;
	xmem_free_function 	free;		/* May be NULL */
};


extern const xmem_t stdc_xmem;


#ifdef __cplusplus
}
#endif

#endif
