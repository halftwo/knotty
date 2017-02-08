#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "escape.h"
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: escape.c,v 1.5 2012/09/20 03:21:47 jiagui Exp $";
#endif



int escape_mem(xio_write_function xio_write, void *xio_cookie,
		const bset_t *meta, escape_callback_function callback,
		const void *data, size_t size)
{
	const char *str = (const char *)data;
	size_t i, last = 0;
	for (i = 0; i < size; ++i)
	{
		if (BSET_TEST(meta, str[i]))
		{
			int n;
			if (i > last)
			{
				if (xio_write(xio_cookie, str + last, i - last) < 0)
					return -1;
			}

			n = callback(xio_write, xio_cookie, str[i]);
			if (n < 0)
			{
				return -1;
			}
			else if (n > 1)
			{
				n = 1;
			}

			last = i + n;
		}
	}

	if (i > last)
	{
		if (xio_write(xio_cookie, str + last, i - last) < 0)
			return -1;
	}

	return 0;
}

int escape_xstr(xio_write_function xio_write, void *xio_cookie,
		const bset_t *meta, escape_callback_function callback,
		const xstr_t *xs)
{
	return escape_mem(xio_write, xio_cookie, meta, callback, xs->data, xs->len);
}

int escape_cstr(xio_write_function xio_write, void *xio_cookie,
		const bset_t *meta, escape_callback_function callback,
		const char *str)
{
	const char *last = str;
	const char *p;
	for (p = str; *p; ++p)
	{
		if (BSET_TEST(meta, *p))
		{
			int n;
			if (p > last)
			{
				if (xio_write(xio_cookie, last, p - last) < 0)
					return -1;
			}

			n = callback(xio_write, xio_cookie, *p);
			if (n < 0)
			{
				return -1;
			}
			else if (n > 1)
			{
				n = 1;
			}

			last = p + n;
		}
	}

	if (p > last)
	{
		if (xio_write(xio_cookie, last, p - last) < 0)
			return -1;
	}

	return 0;
}


int unescape_mem(xio_write_function xio_write, void *xio_cookie,
		char escape, unescape_callback_function callback,
		const void *data, size_t size)
{
	const char *last = (const char *)data;
	const char *end = last + size;
	const char *p;
	while (last < end && (p = (const char *)memchr(last, escape, end - last)) != 0)
	{
		int n;
		if (p > last)
		{
			if (xio_write(xio_cookie, last, p - last) < 0)
				return -1;
		}

		n = callback(xio_write, xio_cookie, (unsigned char *)p, end - p);
		if (n < 0)
		{
			return -1;
		}
		else if (n == 0)
		{
			xio_write(xio_cookie, p, 1);
			n = 1;
		}

		last = p + n;
		if (last > end)
			return -1;
	}

	if (last < end)
	{
		if (xio_write(xio_cookie, last, end - last) < 0)
			return -1;
	}

	return 0;
}

int unescape_xstr(xio_write_function xio_write, void *xio_cookie,
		char escape, unescape_callback_function callback,
		const xstr_t *xs)
{
	return unescape_mem(xio_write, xio_cookie, escape, callback, xs->data, xs->len);
}

static inline char *_strchrnul(const char *str, int ch)
{
#if __GNUC__
	return strchrnul(str, ch);
#else
	for (; *str; ++str)
	{
		if (*str == ch)
			break;
	}
	return str;
#endif
}

int unescape_cstr(xio_write_function xio_write, void *xio_cookie,
		char escape, unescape_callback_function callback,
		const char *str)
{
	const char *last = str;

	while (*last)
	{
		int n;
		const char *p = _strchrnul(last, escape);

		if (p > last)
		{
			if (xio_write(xio_cookie, last, p - last) < 0)
				return -1;
		}

		if (!*p)
			break;

		n = callback(xio_write, xio_cookie, (unsigned char *)p, -1);
		if (n < 0)
		{
			return -1;
		}
		else if (n == 0)
		{
			xio_write(xio_cookie, p, 1);
			n = 1;
		}

		for (--n, ++p; n > 0; --n, ++p)
		{
			if (!*p)
				return -1;
		}

		last = p;
	}

	return 0;
}


ssize_t backslash_escape_xstr(iobuf_t *sink, const xstr_t *xs, const xstr_t *meta,
			const xstr_t *subst_array/*NULL*/, size_t subst_size/*0*/)
{
	size_t num = 0;
	xstr_t s = *xs;
	ssize_t rc;

	if (meta->len == 0)
	{
		return iobuf_write(sink, s.data, s.len);
	}

	while (s.len)
	{
		char ch;
		size_t pos, idx;

		if (meta->len > 1)
		{
			for (pos = 0; pos < s.len; ++pos)
			{
				ch = s.data[pos];
				for (idx = 0; idx < meta->len; ++idx)
				{
					if (ch == meta->data[idx])
					{
						goto found;
					}
				}
			}
		}
		else
		{
			unsigned char *ptr = (unsigned char *)memchr(s.data, meta->data[0], s.len);
			if (ptr)
			{
				ch = meta->data[0];
				pos = ptr - s.data;
				idx = 0;
				goto found;
			}
		}

		rc = iobuf_write(sink, s.data, s.len);
		if (rc < 0)
			return rc;
		num += rc;
		break;
	found:
		if (pos > 0)
		{
			rc = iobuf_write(sink, s.data, pos);
			if (rc < 0)
				return rc;
			num += rc;
		}

		if (idx < subst_size)
		{
			const xstr_t *sbt = &subst_array[idx];
			char buf[16];

			buf[0] = '\\';
			if (sbt->len <= 1)
			{
				buf[1] = sbt->len ? sbt->data[0] : ch;
				rc = iobuf_write(sink, buf, 2);
				if (rc < 0)
					return rc;
				num += rc;
			}
			else if (sbt->len < 16)
			{
				ssize_t n = sbt->len;
				while (n-- > 0)
				{
					buf[n + 1] = sbt->data[n];
				}
				rc = iobuf_write(sink, buf, 1 + sbt->len);
				if (rc < 0)
					return rc;
				num += rc;
			}
			else
			{
				rc = iobuf_putc(sink, buf[0]);
				if (rc < 0)
					return rc;
				num += rc;

				rc = iobuf_write(sink, sbt->data, sbt->len);
				if (rc < 0)
					return rc;
				num += rc;
			}
		}
		else
		{
			char buf[2];

			buf[0] = '\\';
			buf[1] = ch;
			rc = iobuf_write(sink, buf, 2);
			if (rc < 0)
				return rc;
			num += rc;
		}

		++pos;
		xstr_advance(&s, pos);
	}

	return num;
}

