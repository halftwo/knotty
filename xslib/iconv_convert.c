#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "iconv_convert.h"
#include <errno.h>
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: iconv_convert.c,v 1.4 2015/05/13 06:19:49 gremlin Exp $";
#endif


#define ICONV_CLEAR(CD)		iconv((CD), NULL, NULL, NULL, NULL)


int iconv_convert(iconv_t cd, const char *in, size_t inlen, size_t *in_used/*NULL*/,
			char **out, size_t *out_size, size_t *out_used/*NULL*/)
{
	char *iptr, *optr, *p;
	size_t ileft, oleft;
	size_t iuse, ouse;
	size_t outlen;

	if (*out == NULL || *out_size == 0)
	{
		size_t n = inlen >= 256 ? inlen + 1 : 256;
		p = (char *)malloc(n);
		if (p == NULL)
		{
			if (in_used)
				*in_used = 0;
			if (out_used)
				*out_used = 0;

			return -1;
		}
		*out = p;
		*out_size = n;
	}
	outlen = *out_size - 1;

	iptr = (char *)in;
	ileft = inlen;
	optr = *out;
	oleft = outlen;
	while (ileft > 0)
	{
		ssize_t r;

		r = iconv(cd, &iptr, &ileft, &optr, &oleft);
		if (r < 0)
		{
			iuse = inlen - ileft;
			ouse = outlen - oleft;
			(*out)[ouse] = 0;

			if (in_used)
				*in_used = iuse;
			if (out_used)
				*out_used = ouse;

			if (errno == E2BIG)
			{
				size_t n = 256 + (iuse > 0 ? ((inlen + 4.0) / iuse * ouse) : ouse);
				p = (char *)realloc(*out, n);
				if (p == NULL)
				{
					return -1;
				}
				*out = p;
				*out_size = n;
				outlen = *out_size - 1;
				optr = p + ouse;
				oleft = outlen - ouse;
			}
			else
			{
				ICONV_CLEAR(cd);
				return -1;
			}
		}
	}

	ouse = outlen - oleft;
	(*out)[ouse] = 0;

	if (in_used)
		*in_used = inlen;
	if (out_used)
		*out_used = ouse;

	return 0;
}


int iconv_write(iconv_t cd, const char *in, size_t inlen, size_t *in_used/*NULL*/,
			xio_write_function xwrite, void *wcookie)
{
	char obuf[4096];
	char *iptr, *optr;
	size_t ileft, oleft;

	iptr = (char *)in;
	ileft = inlen;
	while (ileft > 0)
	{
		ssize_t r;

		optr = obuf;
		oleft = sizeof(obuf);
		r = iconv(cd, &iptr, &ileft, &optr, &oleft);

		if (in_used)
			*in_used = inlen - ileft;

		if (r < 0)
		{
			if (errno != E2BIG)
			{
				ICONV_CLEAR(cd);
				return -1;
			}
		}

		r = xwrite(wcookie, obuf, sizeof(obuf) - oleft);
		if (r != (ssize_t)(sizeof(obuf) - oleft))
			return -1;
	}
	return 0;
}


int iconv_pipe(iconv_t cd, xio_read_function xread, void *rcookie, size_t *in_used/*NULL*/,
			xio_write_function xwrite, void *wcookie)
{
	char ibuf[2048], obuf[2048];
	char *iptr, *optr;
	size_t ileft, oleft;
	size_t inlen;

	if (in_used)
		*in_used = 0;

	inlen = 0;
	while (1)
	{
		ssize_t r;

		r = xread(rcookie, ibuf + inlen, sizeof(ibuf) - inlen);
		if (r <= 0)
		{
			if (r == 0 && inlen == 0)
				break;
			return -1;
		}
		inlen += r;

		iptr = ibuf;
		ileft = inlen;
		optr = obuf;
		oleft = sizeof(obuf);
		r = iconv(cd, &iptr, &ileft, &optr, &oleft);

		if (in_used)
			*in_used += inlen - ileft;

		if (r < 0)
		{
			if (errno != EINVAL && errno != E2BIG)
			{
				ICONV_CLEAR(cd);
				return -1;
			}
		}

		if (ileft)
			memmove(ibuf, iptr, ileft);
		inlen = ileft;

		r = xwrite(wcookie, obuf, sizeof(obuf) - oleft);
		if (r != (ssize_t)(sizeof(obuf) - oleft))
			return -1;
	}
	return 0;
}

