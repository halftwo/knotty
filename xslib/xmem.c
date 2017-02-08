#include "xmem.h"
#include <stdlib.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xmem.c,v 1.3 2010/05/28 10:28:21 jiagui Exp $";
#endif


static void *_stdc_alloc(void *dummy, size_t size)
{
	return malloc(size);
}

static void _stdc_free(void *dummy, void *ptr)
{
	free(ptr);
}

const xmem_t stdc_xmem = {
	_stdc_alloc,
	_stdc_free,
};


