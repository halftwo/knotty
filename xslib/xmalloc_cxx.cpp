#include "xmalloc.h"
#include "XError.h"
#include <stdlib.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xmalloc_cxx.cpp,v 1.2 2011/01/26 04:44:14 jiagui Exp $";
#endif


void *xmalloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		throw XERROR_FMT(XMemoryError, "malloc(%zu)", size);
	return p;
}

void *xmalloc0(size_t size)
{
	void *p = calloc(1, size);
	if (!p)
		throw XERROR_FMT(XMemoryError, "calloc(1, %zu)", size);
	return p;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);
	if (!p)
		throw XERROR_FMT(XMemoryError, "calloc(%zu, %zu)", nmemb, size);
	return p;
}

void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (!p)
		throw XERROR_FMT(XMemoryError, "realloc(%p, %zu)", ptr, size);
	return p;
}

void xfree(void *ptr)
{
	if (ptr)
		free(ptr);
}

