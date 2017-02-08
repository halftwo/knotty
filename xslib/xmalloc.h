/* $Id: xmalloc.h,v 1.5 2013/11/14 02:30:39 gremlin Exp $ */
#ifndef XMALLOC_H_
#define XMALLOC_H_

#include "xsdef.h"
#include <stddef.h>


/*
  The following functions have both C version and C++ version.
  The C++ version may throw exception XMemoryError which is defined in 
	XError.h
 */

void *xmalloc(size_t size) XS_C_MALLOC;
void *xmalloc0(size_t size) XS_C_MALLOC;
void *xcalloc(size_t nmemb, size_t size) XS_C_MALLOC;
void *xrealloc(void *ptr, size_t size);
void xfree(void *ptr);



#endif
