#include "xmalloc.h"
#include "xlog.h"
#include <stdlib.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xmalloc.c,v 1.3 2013/11/14 02:30:39 gremlin Exp $";
#endif


void *xmalloc(size_t size)
{
	void *p = malloc(size);
	if (!p)
		xlog(XLOG_ALERT, "malloc(%lu) failed", (unsigned long)size);
	return p;
}

void *xmalloc0(size_t size)
{
	void *p = calloc(1, size);
	if (!p)
		xlog(XLOG_ALERT, "calloc(1, %lu) failed", (unsigned long)size);
	return p;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);
	if (!p)
		xlog(XLOG_ALERT, "calloc(%lu, %lu) failed", (unsigned long)nmemb, (unsigned long)size);
	return p;
}

void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (!p)
		xlog(XLOG_ERROR, "realloc(%p, %lu) failed", ptr, (unsigned long)size);
	return p;
}

void xfree(void *ptr)
{
	if (ptr)
		free(ptr);
}

