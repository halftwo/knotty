/* $Id: hex.c,v 1.9 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2007-03-23
 */
#include "hex.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: hex.c,v 1.9 2012/09/20 03:21:47 jiagui Exp $";
#endif

static char _HEX[] = "0123456789ABCDEF";
static char _hex[] = "0123456789abcdef";

#define SPC	-99

static signed char _tab[256] = {
      -1, -1, -1, -1, -1, -1, -1, -1, -1,SPC,SPC,SPC,SPC,SPC, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     SPC, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
       0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
      -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};


ssize_t hexlify_upper(char *dst, const void *src, size_t size)
{
	ssize_t rc = hexlify_upper_nz(dst, src, size);
	dst[rc] = 0;
	return rc;
}

ssize_t hexlify_upper_nz(char *dst, const void *src, size_t size)
{
	char *d = dst;
	unsigned char *s = (unsigned char *)src;
	ssize_t i;
	for (i = 0; i < (ssize_t)size; ++i)
	{
		int b1 = (s[i] >> 4);
		int b2 = (s[i] & 0x0f);
		*d++ = _HEX[b1];
		*d++ = _HEX[b2];
	}
	return d - dst;
}

ssize_t hexlify(char *dst, const void *src, size_t size)
{
	ssize_t rc = hexlify_nz(dst, src, size);
	dst[rc] = 0;
	return rc;
}

ssize_t hexlify_nz(char *dst, const void *src, size_t size)
{
	char *d = dst;
	unsigned char *s = (unsigned char *)src;
	ssize_t i;
	for (i = 0; i < (ssize_t)size; ++i)
	{
		int b1 = (s[i] >> 4);
		int b2 = (s[i] & 0x0f);
		*d++ = _hex[b1];
		*d++ = _hex[b2];
	}
	return d - dst;
}

ssize_t unhexlify(void *dst, const char *src, size_t size)
{
	unsigned char *s = (unsigned char *)src;
	unsigned char *end = (ssize_t)size < 0 ? (unsigned char *)-1 : s + size;
	char *d = (char *)dst;
	int b1, b2;

	while (s + 2 <= end && (b1 = _tab[*s++]) >= 0 && (b2 = _tab[*s++]) >= 0)
	{
		*d++ = (b1 << 4) + b2;
	}

	if (s != end && s[-1] != 0)
	{
		return -(s - 1 - (unsigned char *)src);
	}

	return d - (char *)dst;
}

ssize_t unhexlify_ignore_space(void *dst, const char *src, size_t size)
{
	unsigned char *s = (unsigned char *)src;
	unsigned char *end = (ssize_t)size < 0 ? (unsigned char *)-1 : s + size;
	char *d = (char *)dst;
	int odd, b1;

	odd = 0;
	while (s < end)
	{
		int x = _tab[*s++];
		if (x < 0)
		{
			if (x == -1)
				break;
			while (s < end && (x = _tab[*s++]) == SPC)
				continue;
			if (x < 0)
				break;
		}

		odd ^= 1;
		if (odd)
			b1 = x;
		else
			*d++ = (b1 << 4) + x;
	}

	if (s != end && s[-1] != 0)
		return -(s - 1 - (unsigned char *)src);

	if (odd)
		return -(s - (unsigned char *)src);

	return d - (char *)dst;
}


#ifdef TEST_HEX

#include <stdio.h>
#include <assert.h>
#include <string.h>

int main()
{
	char buf[1024];
	char *src = "hello, world!";
	int len = strlen(src);
	int i;

	for (i = 0; i < 1024*1024; ++i)
	{
		hexlify(buf, src, len);
		unhexlify(buf, buf, -1);
	}

	len = hexlify(buf, src, len);
	printf("%d\t%s\t%s\n", len, src, buf);

	len = unhexlify(buf, buf, -1);
	buf[len] = 0;
	printf("%d\t%s\t%s\n", len, src, buf);

	return 0;
}

#endif
