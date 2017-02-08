/* $Id: strhash.c,v 1.4 2009/02/09 04:46:59 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-06-26
 */
#include "strhash.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: strhash.c,v 1.4 2009/02/09 04:46:59 jiagui Exp $";
#endif

unsigned int strhash(const char *str, unsigned int initval)
{
	unsigned int c;
	unsigned int h = initval;
	unsigned char *p = (unsigned char *)str;
	while ((c = *p++) != 0)
		h += (h << 7) + c + 987654321;
	return h;
}

unsigned int memhash(const void *mem, size_t n, unsigned int initval)
{
	unsigned int h = initval;
	unsigned char *p = (unsigned char *)mem;
	unsigned char *end = (unsigned char *)mem + n;
	while (p < end)
		h += (h << 7) + (*p++) + 987654321;
	return h;
}

