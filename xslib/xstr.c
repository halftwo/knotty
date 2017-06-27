#ifndef _ISOC99_SOURCE
#define _ISOC99_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "xstr.h"
#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xstr.c,v 1.45 2015/07/10 06:10:49 gremlin Exp $";
#endif

const xstr_t xstr_null = { 0, 0 };

#define ISSPACE(ch)	BSET_TEST(&space_bset, ch)


static inline void _advance(xstr_t *xs, size_t n)
{
	xs->data += n;
	xs->len -= n;
}

static inline void *_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen)
{
	return memmem(haystack, haystacklen, needle, needlelen);
}

static inline void *_seek_char(const void *data, size_t len, char ch)
{
	return memchr(data, ch, len);
}

static inline void *_seek_not_char(const void *data, size_t len, char ch)
{
	char *end = (char *)data + len;
	char *p;
	for (p = (char *)data; p < end; ++p)
	{
		if (ch != *p)
			return p;
	}
	return NULL;
}

static inline void *_seek_in_mem(const void *data, size_t len, const void *chset, size_t n)
{
	char *end = (char *)data + len;
	char *p;
	for (p = (char *)data; p < end; ++p)
	{
		size_t k;
		for (k = 0; k < n; ++k)
		{
			if (*p == ((char *)chset)[k])
				return p;
		}
	}
	return NULL;
}

static inline void *_seek_not_in_mem(const void *data, size_t len, const void *chset, size_t n)
{
	char *end = (char *)data + len;
	char *p = (char *)data;
again:
	if (p < end)
	{
		size_t k;
		for (k = 0; k < n; ++k)
		{
			if (*p == ((char *)chset)[k])
			{
				++p;
				goto again;
			}
		}
		return p;
	}
	return NULL;
}

static inline void *_seek_in_cstr(const void *data, size_t len, const char *chset)
{
	char *end = (char *)data + len;
	char *p;
	for (p = (char *)data; p < end; ++p)
	{
		const char *cs;
		for (cs = chset; *cs; ++cs)
		{
			if (*p == *cs)
				return p;
		}
	}
	return NULL;
}

static inline void *_seek_not_in_cstr(const void *data, size_t len, const char *chset)
{
	char *end = (char *)data + len;
	char *p = (char *)data;
again:
	if (p < end)
	{
		const char *cs;
		for (cs = chset; *cs; ++cs)
		{
			if (*p == *cs)
			{
				++p;
				goto again;
			}
		}
		return p;
	}
	return NULL;
}

static inline void *_rseek_not_in_mem(const void *data, size_t len, const void *chset, size_t n)
{
	char *p = (char *)data + len;
again:
	if (p > (char *)data)
	{
		size_t k;
		--p;
		for (k = 0; k < n; ++k)
		{
			if (*p == ((char *)chset)[k])
			{
				goto again;
			}
		}
		return p;
	}
	return NULL;
}

static inline void *_rseek_not_in_cstr(const void *data, size_t len, const char *chset)
{
	char *p = (char *)data + len;
again:
	if (p > (char *)data)
	{
		const char *cs;
		--p;
		for (cs = chset; *cs; ++cs)
		{
			if (*p == *cs)
			{
				goto again;
			}
		}
		return p;
	}
	return NULL;
}


inline xstr_t xstr_dup_mem(const void *s, size_t n)
{
	xstr_t xs;
	if (n > 0 && (xs.data = malloc(n)) != NULL)
	{
		xs.len = n;
		memcpy(xs.data, s, n);
	}
	else
	{
		xs = xstr_null;
	}
	return xs;
}

xstr_t xstr_dup(const xstr_t *str)
{
	return xstr_dup_mem(str->data, str->len);
}

xstr_t xstr_dup_cstr(const char *str)
{
	return xstr_dup_mem(str, strlen(str));
}

inline char *strdup_mem(const void *s, size_t n)
{
	char *dst = (char *)malloc(n + 1);
	if (dst)
	{
		if (n > 0)
		{
			memcpy(dst, s, n);
		}
		dst[n] = 0;
	}
	return dst;
}

char *strdup_xstr(const xstr_t *str)
{
	return strdup_mem(str->data, str->len);
}

size_t xstr_copy_to(const xstr_t *str, void *buf, size_t n)
{
	if (n > 0)
	{
		n = (n <= str->len) ? n - 1 : str->len;
		if (n > 0)
			memcpy(buf, str->data, n);
		((uint8_t*)buf)[n] = 0;
	}
	return n;
}

inline int xstr_compare_mem(const xstr_t *xs1, const void *s2, size_t n)
{
	size_t k;
	int r, z;

	if (xs1->len < n)
	{
		k = xs1->len;
		z = -1;
	}
	else
	{
		k = n;
		z = 1;
	}
	r = k > 0 ? memcmp(xs1->data, s2, k) : 0;
	return (r || xs1->len == n) ? r : z;
}

int xstr_compare(const xstr_t *xs1, const xstr_t *xs2)
{
	return xstr_compare_mem(xs1, xs2->data, xs2->len);
}

inline int xstr_compare_cstr(const xstr_t *xs1, const char *s2)
{
	const uint8_t *s1 = xs1->data;
	size_t n = xs1->len;

	while (n-- > 0)
	{
		int c1 = (uint8_t) *s1++;
		int c2 = (uint8_t) *s2++;

		if (c2 == 0)
			return 1;

		if (c1 != c2)
			return c1 - c2;
	}

	return (*s2) ? -1 : 0;
}

inline bool xstr_equal_mem(const xstr_t *xs1, const void *s2, size_t n)
{
	return (xs1->len == n) && (n == 0 || memcmp(xs1->data, s2, n) == 0);
}

bool xstr_equal(const xstr_t *xs1, const xstr_t *xs2)
{
	return xstr_equal_mem(xs1, xs2->data, xs2->len);
}

bool xstr_equal_cstr(const xstr_t *s1, const char *s2)
{
	return (xstr_compare_cstr(s1, s2) == 0);
}

inline int xstr_alphabet_compare_mem(const xstr_t *xs1, const void *ptr, size_t n)
{
	const uint8_t *s1, *s2;
	size_t k;
	int z;
	bool len_equal = (xs1->len == n);
	int delta = 0;

	if (xs1->len < n)
	{
		k = xs1->len;
		z = -1;
	}
	else
	{
		k = n;
		z = 1;
	}

	for (s1 = xs1->data, s2 = (const uint8_t *)ptr; k; --k)
	{
		int c1 = (uint8_t)*s1++;
		int c2 = (uint8_t)*s2++;

		if (c1 != c2)
		{
			if (len_equal && !delta)
				delta = c1 -c2;

			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c2 >= 'A' && c2 <= 'Z')
				c2 |= 0x20;

			if (c1 != c2)
				return c1 - c2;
		}
	}

	if (len_equal)
	{
		return delta;
	}

	return z;
}

int xstr_alphabet_compare(const xstr_t *xs1, const xstr_t *xs2)
{
	return xstr_alphabet_compare_mem(xs1, xs2->data, xs2->len);
}

inline int xstr_alphabet_compare_cstr(const xstr_t *xs1, const char *s2)
{
	const uint8_t *s1 = xs1->data;
	size_t n = xs1->len;
	int delta = 0;

	while (n-- > 0)
	{
		int c1 = (uint8_t)*s1++;
		int c2 = (uint8_t)*s2++;

		if (c2 == 0)
			return 1;

		if (c1 != c2)
		{
			if (!delta)
				delta = c1 - c2;

			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c2 >= 'A' && c2 <= 'Z')
				c2 |= 0x20;

			if (c1 != c2)
				return c1 - c2;
		}
	}

	if (*s2)
		return -1;
	
	return delta;
}

inline int xstr_case_compare_mem(const xstr_t *xs1, const void *ptr, size_t n)
{
	const uint8_t *s1, *s2;
	size_t k;
	int z;

	if (xs1->len < n)
	{
		k = xs1->len;
		z = -1;
	}
	else
	{
		k = n;
		z = 1;
	}

	for (s1 = xs1->data, s2 = (const uint8_t *)ptr; k; --k)
	{
		int c1 = (uint8_t)*s1++;
		int c2 = (uint8_t)*s2++;

		if (c1 != c2)
		{
			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c2 >= 'A' && c2 <= 'Z')
				c2 |= 0x20;

			if (c1 != c2)
				return c1 - c2;
		}
	}

	return xs1->len == n ? 0 : z;
}

int xstr_case_compare(const xstr_t *xs1, const xstr_t *xs2)
{
	return xstr_case_compare_mem(xs1, xs2->data, xs2->len);
}

inline int xstr_case_compare_cstr(const xstr_t *xs1, const char *s2)
{
	const uint8_t *s1 = xs1->data;
	size_t n = xs1->len;

	while (n-- > 0)
	{
		int c1 = (uint8_t)*s1++;
		int c2 = (uint8_t)*s2++;

		if (c2 == 0)
			return 1;

		if (c1 != c2)
		{
			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c2 >= 'A' && c2 <= 'Z')
				c2 |= 0x20;

			if (c1 != c2)
				return c1 - c2;
		}
	}

	return (*s2) ? -1 : 0;
}

static inline bool _mem_case_equal(const void *p1, const void *p2, size_t n)
{
	const uint8_t *s1, *s2;
	for (s1 = (const uint8_t *)p1, s2 = (const uint8_t *)p2; n; --n)
	{
		int c1 = (uint8_t)*s1++;
		int c2 = (uint8_t)*s2++;

		if (c1 != c2)
		{
			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c2 >= 'A' && c2 <= 'Z')
				c2 |= 0x20;

			if (c1 != c2)
				return false;
		}
	}
	return true;
}

bool xstr_case_equal_mem(const xstr_t *xs1, const void *ptr, size_t n)
{
	return (xs1->len == n) && (n == 0 || _mem_case_equal(xs1->data, ptr, n));
}

bool xstr_case_equal(const xstr_t *xs1, const xstr_t *xs2)
{
	return (xs1->len == xs2->len) && _mem_case_equal(xs1->data, xs2->data, xs2->len);
}

bool xstr_case_equal_cstr(const xstr_t *s1, const char *s2)
{
	return (xstr_case_compare_cstr(s1, s2) == 0);
}


ssize_t xstr_find_char(const xstr_t *xs, ssize_t pos, char ch)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if ((size_t)pos < xs->len)
	{
		uint8_t *s = (uint8_t *)memchr(xs->data + pos, ch, xs->len - pos);
		return s ? s - xs->data : -1;
	}
	return -1;
}

ssize_t xstr_find_not_char(const xstr_t *xs, ssize_t pos, char ch)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	for (; pos < xs->len; ++pos)
	{
		if (xs->data[pos] != ch)
			return pos;
	}
	return -1;
}

ssize_t xstr_rfind_char(const xstr_t *xs, ssize_t pos, char ch)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if ((size_t)pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	if (pos >= 0)
	{
		uint8_t *s = (uint8_t *)memrchr(xs->data, ch, pos + 1);
		return s ? s - xs->data : -1;
	}
	return -1;
}

ssize_t xstr_rfind_not_char(const xstr_t *xs, ssize_t pos, char ch)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if (pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	for (; pos >= 0; --pos)
	{
		if (xs->data[pos] != ch)
			return pos;
	}
	return -1;
}


inline ssize_t xstr_find_mem(const xstr_t *xs, ssize_t pos, const void *needle, size_t n)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if (n > 0)
	{
		if ((size_t)pos < xs->len && n <= xs->len - pos)
		{
			uint8_t *p = (uint8_t *)_memmem(xs->data + pos, xs->len - pos, needle, n);
			if (p)
				return p - xs->data;
		}
	}
	else
		return (size_t)pos <= xs->len ? pos : -1;

	return -1;
}

ssize_t xstr_find(const xstr_t *xs, ssize_t pos, const xstr_t *needle)
{
	return xstr_find_mem(xs, pos, needle->data, needle->len);
}

ssize_t xstr_find_cstr(const xstr_t *xs, ssize_t pos, const char *needle)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if (needle[0])
	{
		if ((size_t)pos < xs->len)
		{
			size_t n = strlen(needle);
			if (n <= xs->len - pos)
			{
				uint8_t *p = (uint8_t *)_memmem(xs->data + pos, xs->len - pos, needle, n);
				if (p)
					return p - xs->data;
			}
		}
	}
	else
		return (size_t)pos <= xs->len ? pos : -1;

	return -1;
}

ssize_t xstr_case_find_mem(const xstr_t *xs, ssize_t pos, const void *needle, size_t n)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if (n > 0)
	{
		if ((size_t)pos < xs->len && n <= xs->len - pos)
		{
			const uint8_t *s = xs->data + pos;
			const uint8_t *end = xs->data + xs->len - n;
			int c2 = *(uint8_t*)needle;
			int c2low = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
			int c2up = (c2 >= 'a' && c2 <= 'z') ? (c2 & ~0x20) : c2;

			needle = (uint8_t *)needle + 1;	/* make C++ compiler happy */
			--n;
			for (; s <= end; ++s)
			{
				int c1 = (uint8_t)*s;
				if (c1 == c2low || c1 == c2up)
				{
					if (n == 0 || _mem_case_equal(s + 1, needle, n))
						return (s - xs->data);
				}
			}
		}
	}
	else
		return (size_t)pos <= xs->len ? pos : -1;

	return -1;
}

ssize_t xstr_case_find(const xstr_t *xs, ssize_t pos, const xstr_t *needle)
{
	return xstr_case_find_mem(xs, pos, needle->data, needle->len);
}

ssize_t xstr_case_find_cstr(const xstr_t *xs, ssize_t pos, const char *needle)
{
	xstr_t nd = XSTR_C(needle);
	return xstr_case_find(xs, pos, &nd);
}

inline ssize_t xstr_rfind_mem(const xstr_t *xs, ssize_t pos, const void *needle, size_t n)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}

	if (n > 0)
	{
		if ((size_t)pos >= xs->len)
			pos = xs->len - 1;

		if (n == 1)
		{
			uint8_t *p = (uint8_t *)memrchr(xs->data, *(uint8_t *)needle, pos + 1);
			return p ? p - xs->data : -1;
		}

		if (n <= pos + 1)
		{
			const uint8_t *s = xs->data + pos + 1 - n;
			int c2 = *(uint8_t *)needle;

			needle = (uint8_t *)needle + 1;	/* make C++ compiler happy */
			--n;
			for (; s >= xs->data; --s)
			{
				int c1 = (uint8_t)*s;
				if (c1 == c2)
				{
					if (memcmp(s + 1, needle, n) == 0)
						return (s - xs->data);
				}
			}
		}
	}
	else
		return (size_t)pos <= xs->len ? pos : -1;

	return -1;
}

ssize_t xstr_rfind(const xstr_t *xs, ssize_t pos, const xstr_t *needle)
{
	return xstr_rfind_mem(xs, pos, needle->data, needle->len);
}

ssize_t xstr_rfind_cstr(const xstr_t *xs, ssize_t pos, const char *needle)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}

	if (needle[0])
	{
		size_t n;
		if (pos >= xs->len)
			pos = xs->len - 1;

		if (needle[1] == 0)
		{
			uint8_t *p = (uint8_t *)memrchr(xs->data, needle[0], pos + 1);
			return p ? p - xs->data : -1;
		}

		n = strlen(needle);
		if (n <= pos + 1)
		{
			const uint8_t *s = xs->data + pos + 1 - n;
			int c2 = (uint8_t)needle[0];

			++needle;
			--n;
			for (; s >= xs->data; --s)
			{
				int c1 = (uint8_t)*s;
				if (c1 == c2)
				{
					if (memcmp(s + 1, needle, n) == 0)
						return (s - xs->data);
				}
			}
		}
	}
	else
		return (size_t)pos <= xs->len ? pos : -1;

	return -1;
}

ssize_t xstr_case_rfind_mem(const xstr_t *xs, ssize_t pos, const void *needle, size_t n)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}

	if (n > 0)
	{
		if ((size_t)pos >= xs->len)
			pos = xs->len - 1;

		if (n <= pos + 1)
		{
			const uint8_t *s = xs->data + pos + 1 - n;
			int c2 = *(uint8_t *)needle;
			int c2low = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;
			int c2up = (c2 >= 'a' && c2 <= 'z') ? (c2 & ~0x20) : c2;

			needle = (uint8_t *)needle + 1;	/* make C++ compiler happy */
			--n;
			for (; s >= xs->data; --s)
			{
				int c1 = (uint8_t)*s;
				if (c1 == c2low || c1 == c2up)
				{
					if (n == 0 || _mem_case_equal(s + 1, needle, n))
						return (s - xs->data);
				}
			}
		}
	}
	else
		return (size_t)pos <= xs->len ? pos : -1;

	return -1;
}

ssize_t xstr_case_rfind(const xstr_t *xs, ssize_t pos, const xstr_t *needle)
{
	return xstr_case_rfind_mem(xs, pos, needle->data, needle->len);
}

ssize_t xstr_case_rfind_cstr(const xstr_t *xs, ssize_t pos, const char *needle)
{
	return xstr_case_rfind_mem(xs, pos, needle, strlen(needle));
}

inline ssize_t xstr_find_in_mem(const xstr_t *xs, ssize_t pos, const void *chset, size_t n)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if ((size_t)pos < xs->len)
	{
		uint8_t *p;

		if (n == 0)
		{
			return -1;
		}
		else if (n == 1)
		{
			p = (uint8_t *)memchr(xs->data + pos, *(uint8_t *)chset, xs->len - pos);
		}
		else
		{
			p = (uint8_t *)_seek_in_mem(xs->data + pos, xs->len - pos, chset, n);
		}

		return p ? p - xs->data : -1;
	}

	return -1;
}

ssize_t xstr_find_in(const xstr_t *xs, ssize_t pos, const xstr_t *chset)
{
	return xstr_find_in_mem(xs, pos, chset->data, chset->len);
}

ssize_t xstr_find_in_cstr(const xstr_t *xs, ssize_t pos, const char *chset)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if (pos < xs->len)
	{
		uint8_t *p;

		if (chset[0] == 0)
		{
			return -1;
		}
		else if (chset[1] == 0)
		{
			p = (uint8_t *)memchr(xs->data + pos, chset[0], xs->len - pos);
		}
		else
		{
			p = (uint8_t *)_seek_in_cstr(xs->data + pos, xs->len - pos, chset);
		}

		return p ? p - xs->data : -1;
	}

	return -1;
}

ssize_t xstr_find_in_bset(const xstr_t *xs, ssize_t pos, const bset_t *bset)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	while (pos < xs->len)
	{
		if (BSET_TEST(bset, xs->data[pos]))
			return pos;
		++pos;
	}

	return -1;
}

inline ssize_t xstr_find_not_in_mem(const xstr_t *xs, ssize_t pos, const void *chset, size_t n)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if ((size_t)pos < xs->len)
	{
		uint8_t *p;

		if (n == 0)
		{
			return pos;
		}
		else if (n == 1)
		{
			p = (uint8_t *)_seek_not_char(xs->data + pos, xs->len - pos, *(char *)chset);
		}
		else
		{
			p = (uint8_t *)_seek_not_in_mem(xs->data + pos, xs->len - pos, chset, n);
		}

		return p ? p - xs->data : -1;
	}

	return -1;
}

ssize_t xstr_find_not_in(const xstr_t *xs, ssize_t pos, const xstr_t *chset)
{
	return xstr_find_not_in_mem(xs, pos, chset->data, chset->len);
}

ssize_t xstr_find_not_in_cstr(const xstr_t *xs, ssize_t pos, const char *chset)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if (pos < xs->len)
	{
		uint8_t *p;

		if (chset[0] == 0)
		{
			return pos;
		}
		else if (chset[1] == 0)
		{
			p = (uint8_t *)_seek_not_char(xs->data + pos, xs->len - pos, chset[0]);
		}
		else
		{
			p = (uint8_t *)_seek_not_in_cstr(xs->data + pos, xs->len - pos, chset);
		}

		return p ? p - xs->data : -1;
	}

	return -1;
}

ssize_t xstr_find_not_in_bset(const xstr_t *xs, ssize_t pos, const bset_t *bset)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	while ((size_t)pos < xs->len)
	{
		if (!BSET_TEST(bset, xs->data[pos]))
			return pos;
		++pos;
	}

	return -1;
}

inline ssize_t xstr_rfind_in_mem(const xstr_t *xs, ssize_t pos, const void *chset, size_t n)
{
	ssize_t i;

	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if ((size_t)pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	if (n == 0)
		return -1;
	else if (n == 1)
	{
		uint8_t *s = (uint8_t *)memrchr(xs->data, *(uint8_t *)chset, pos + 1);
		return s ? s - xs->data : -1;
	}

	for (i = pos; i >= 0; --i)
	{
		uint8_t ch = xs->data[i];
		ssize_t k;
		for (k = 0; k < (ssize_t)n; ++k)
		{
			if (ch == ((uint8_t *)chset)[k])
				return i;
		}
	}

	return -1;
}

ssize_t xstr_rfind_in(const xstr_t *xs, ssize_t pos, const xstr_t *chset)
{
	return xstr_rfind_in_mem(xs, pos, chset->data, chset->len);
}

ssize_t xstr_rfind_in_cstr(const xstr_t *xs, ssize_t pos, const char *chset)
{
	ssize_t i;

	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if ((size_t)pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	if (chset[0] == 0)
		return -1;
	else if (chset[1] == 0)
	{
		uint8_t *s = (uint8_t *)memrchr(xs->data, chset[0], pos + 1);
		return s ? s - xs->data : -1;
	}

	for (i = pos; i >= 0; --i)
	{
		uint8_t ch = xs->data[i];
		ssize_t k;
		for (k = 0; chset[k]; ++k)
		{
			if (ch == (uint8_t)chset[k])
				return i;
		}
	}

	return -1;
}

ssize_t xstr_rfind_in_bset(const xstr_t *xs, ssize_t pos, const bset_t *bset)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if ((size_t)pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	while (pos >= 0)
	{
		if (BSET_TEST(bset, xs->data[pos]))
			return pos;
		--pos;
	}

	return -1;
}

inline ssize_t xstr_rfind_not_in_mem(const xstr_t *xs, ssize_t pos, const void *chset, size_t n)
{
	ssize_t i;

	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if ((size_t)pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	if (n == 0)
		return pos;
	else if (n == 1)
	{
		uint8_t ch = *(uint8_t *)chset;
		for (i = pos; i >= 0; --i)
		{
			if (xs->data[i] != ch)
				return i;
		}
	}
	else
	{
		i = pos;
	again:
		if (i >= 0)
		{
			uint8_t ch = xs->data[i];
			ssize_t k;
			for (k = 0; k < (ssize_t)n; ++k)
			{
				if (ch == ((uint8_t *)chset)[k])
				{
					--i;
					goto again;
				}
			}
			return i;
		}
	}

	return -1;
}

ssize_t xstr_rfind_not_in(const xstr_t *xs, ssize_t pos, const xstr_t *chset)
{
	return xstr_rfind_not_in_mem(xs, pos, chset->data, chset->len);
}

ssize_t xstr_rfind_not_in_cstr(const xstr_t *xs, ssize_t pos, const char *chset)
{
	ssize_t i;

	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if ((size_t)pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	if (chset[0] == 0)
		return pos;
	else if (chset[1] == 0)
	{
		uint8_t ch = chset[0];
		for (i = pos; i >= 0; --i)
		{
			if (xs->data[i] != ch)
				return i;
		}
	}
	else
	{
		i = pos;
	again:
		if (i >= 0)
		{
			uint8_t ch = xs->data[i];
			ssize_t k;
			for (k = 0; chset[k]; ++k)
			{
				if (ch == chset[k])
				{
					--i;
					goto again;
				}
			}
			return i;
		}
	}

	return -1;
}

ssize_t xstr_rfind_not_in_bset(const xstr_t *xs, ssize_t pos, const bset_t *bset)
{
	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			return -1;
	}
	else if ((size_t)pos >= xs->len)
	{
		pos = xs->len - 1;
	}

	while (pos >= 0)
	{
		if (!BSET_TEST(bset, xs->data[pos]))
			return pos;
		--pos;
	}

	return -1;
}

inline bool xstr_start_with_mem(const xstr_t *xs, const void *prefix, size_t n)
{
	return (n <= xs->len && (n == 0 || memcmp(xs->data, prefix, n) == 0));
}

bool xstr_start_with(const xstr_t *xs, const xstr_t *prefix)
{
	return xstr_start_with_mem(xs, prefix->data, prefix->len);
}

bool xstr_start_with_cstr(const xstr_t *xs, const char *prefix)
{
	size_t i;

	for (i = 0; prefix[i] && i < xs->len; ++i)
	{
		if ((uint8_t)prefix[i] != xs->data[i])
			return false;
	}

	return prefix[i] ? false : true;
}

inline bool xstr_case_start_with_mem(const xstr_t *xs, const void *prefix, size_t n)
{
	return (n <= xs->len) && (n == 0 || _mem_case_equal(xs->data, prefix, n));
}

bool xstr_case_start_with(const xstr_t *xs, const xstr_t *prefix)
{
	return xstr_case_start_with_mem(xs, prefix->data, prefix->len);
}

bool xstr_case_start_with_cstr(const xstr_t *xs, const char *prefix)
{
	size_t i;

	for (i = 0; prefix[i] && i < xs->len; ++i)
	{
		int c1 = (uint8_t)prefix[i];
		int c2 = (uint8_t)xs->data[i];

		if (c1 != c2)
		{
			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c2 >= 'A' && c2 <= 'Z')
				c2 |= 0x20;

			if (c1 != c2)
				return false;
		}
	}

	return prefix[i] ? false : true;
}

inline bool xstr_end_with_mem(const xstr_t *xs, const void *suffix, size_t n)
{
	return (n <= xs->len && (n == 0 || memcmp(xs->data + xs->len - n, suffix, n) == 0));
}

bool xstr_end_with(const xstr_t *xs, const xstr_t *suffix)
{
	return xstr_end_with_mem(xs, suffix->data, suffix->len);
}

bool xstr_end_with_cstr(const xstr_t *xs, const char *suffix)
{
	size_t slen;

	if (suffix[0] == 0)
		return true;

	slen = strlen(suffix);
	if (slen > xs->len)
		return false;

	return (memcmp(xs->data + xs->len - slen, suffix, slen) == 0);
}

inline bool xstr_case_end_with_mem(const xstr_t *xs, const void *suffix, size_t n)
{
	return (n <= xs->len) && (n == 0 || _mem_case_equal(xs->data + xs->len - n, suffix, n));
}

bool xstr_case_end_with(const xstr_t *xs, const xstr_t *suffix)
{
	return xstr_case_end_with_mem(xs, suffix->data, suffix->len);
}

bool xstr_case_end_with_cstr(const xstr_t *xs, const char *suffix)
{
	xstr_t str;
	xstr_t s;

	if (suffix[0] == 0)
		return true;

	s.data = (uint8_t *)suffix;
	s.len = strlen(suffix);
	if (s.len > xs->len)
		return false;

	str.data = xs->data + xs->len - s.len;
	str.len = s.len;
	return xstr_case_equal(&str, &s);
}

xstr_t xstr_slice(const xstr_t *xs, ssize_t start, ssize_t end)
{
	xstr_t sub;

	if (start < 0)
	{
		start += xs->len;
		if (start < 0)
			start = 0;
	}

	if (end < 0)
	{
		end += xs->len;

		if (start <= end)
		{
			sub.data = xs->data + start;
			sub.len = end - start;
		}
		else
		{
			sub.data = (start < xs->len) ? xs->data + start : xs->data + xs->len;
			sub.len = 0;
		}
	}
	else
	{
		if (end > xs->len)
			end = xs->len;

		if (start <= end)
		{
			sub.data = xs->data + start;
			sub.len = end - start;
		}
		else
		{
			sub.data = (start < xs->len) ? xs->data + start : xs->data + xs->len;
			sub.len = 0;
		}
	}

	return sub;
}

size_t xstr_slice_copy_to(const xstr_t *str, ssize_t start, ssize_t end, void *buf, size_t n)
{
	xstr_t tmp = xstr_slice(str, start, end);
	return xstr_copy_to(&tmp, buf, n);
}

xstr_t xstr_prefix(const xstr_t *xs, ssize_t end)
{
	xstr_t sub;

	if (end < 0)
	{
		end += xs->len;
		if (end < 0)
			end = 0;
	}
	else if (end > xs->len)
	{
		end = xs->len;
	}

	sub.data = xs->data;
	sub.len = end;
	return sub;
}

xstr_t xstr_suffix(const xstr_t *xs, ssize_t start)
{
	xstr_t sub;

	if (start < 0)
	{
		start += xs->len;
		if (start < 0)
			start = 0;
	}
	else if (start > xs->len)
	{
		start = xs->len;
	}

	sub.data = xs->data + start;
	sub.len = xs->len - start;
	return sub;
}

xstr_t xstr_substr(const xstr_t *xs, ssize_t pos, size_t length)
{
	xstr_t sub;

	if (pos < 0)
	{
		pos += xs->len;
		if (pos < 0)
			pos = 0;
	}

	if ((size_t)pos < xs->len)
	{
		sub.data = xs->data + pos;
		sub.len = xs->len - pos;
		if (sub.len > length)
			sub.len = length;
	}
	else
	{
		sub.data = xs->data + xs->len;
		sub.len = 0;
	}

	return sub;
}

size_t xstr_substr_copy_to(const xstr_t *str, ssize_t pos, size_t length, void *buf, size_t n)
{
	xstr_t tmp = xstr_substr(str, pos, length);
	return xstr_copy_to(&tmp, buf, n);
}

xstr_t *xstr_advance(xstr_t *xs, size_t n)
{
	_advance(xs, n < xs->len ? n : xs->len);
	return xs;
}

xstr_t *xstr_lstrip_mem(xstr_t *xs, const void *rubbish, size_t n)
{
	uint8_t *p = (uint8_t *)_seek_not_in_mem(xs->data, xs->len, rubbish, n);
	if (p != xs->data)
		_advance(xs, p ? p - xs->data : xs->len);
	return xs;
}

xstr_t *xstr_lstrip(xstr_t *xs, const xstr_t *rubbish)
{
	return xstr_lstrip_mem(xs, rubbish->data, rubbish->len);
}

xstr_t *xstr_lstrip_cstr(xstr_t *xs, const char *rubbish)
{
	uint8_t *p = (uint8_t *)_seek_not_in_cstr(xs->data, xs->len, rubbish);
	if (p != xs->data)
		_advance(xs, p ? p - xs->data : xs->len);
	return xs;
}

inline xstr_t *xstr_lstrip_char(xstr_t *xs, char rubbish)
{
	while (xs->len > 0 && xs->data[0] == rubbish)
	{
		xs->data++;
		xs->len--;
	}
	return xs;
}

xstr_t *xstr_rstrip_mem(xstr_t *xs, const void *rubbish, size_t n)
{
	uint8_t *p = (uint8_t *)_rseek_not_in_mem(xs->data, xs->len, rubbish, n);
	if (p)
	{
		xs->len = p + 1 - xs->data;
	}
	else
	{
		xs->len = 0;
	}
	return xs;
}

xstr_t *xstr_rstrip(xstr_t *xs, const xstr_t *rubbish)
{
	return xstr_rstrip_mem(xs, rubbish->data, rubbish->len);
}

xstr_t *xstr_rstrip_cstr(xstr_t *xs, const char *rubbish)
{
	uint8_t *p = (uint8_t *)_rseek_not_in_cstr(xs->data, xs->len, rubbish);
	if (p)
	{
		xs->len = p + 1 - xs->data;
	}
	else
	{
		xs->len = 0;
	}
	return xs;
}

inline xstr_t *xstr_rstrip_char(xstr_t *xs, char rubbish)
{
	while (xs->len > 0 && xs->data[xs->len - 1] == rubbish)
	{
		xs->len--;
	}
	return xs;
}


xstr_t *xstr_strip_mem(xstr_t *xs, const void *rubbish, size_t n)
{
	uint8_t *p = (uint8_t *)_rseek_not_in_mem(xs->data, xs->len, rubbish, n);
	if (p)
	{
		uint8_t *s;
		++p;
		s = (uint8_t *)_seek_not_in_mem(xs->data, p - xs->data, rubbish, n);
		xs->data = s;
		xs->len = p - s;
	}
	else
	{
		xs->len = 0;
	}

	return xs;
}

xstr_t *xstr_strip(xstr_t *xs, const xstr_t *rubbish)
{
	return xstr_strip_mem(xs, rubbish->data, rubbish->len);
}

xstr_t *xstr_strip_cstr(xstr_t *xs, const char *rubbish)
{
	uint8_t *p = (uint8_t *)_rseek_not_in_cstr(xs->data, xs->len, rubbish);
	if (p)
	{
		uint8_t *s;
		++p;
		s = (uint8_t *)_seek_not_in_cstr(xs->data, p - xs->data, rubbish);
		xs->data = s;
		xs->len = p - s;
	}
	else
	{
		xs->len = 0;
	}

	return xs;
}

inline xstr_t *xstr_lstrip_bset(xstr_t *xs, const bset_t *rubbish)
{
	if (xs->len && BSET_TEST(rubbish, xs->data[0]))
	{
		size_t i = 1;
		while (i < xs->len && BSET_TEST(rubbish, xs->data[i]))
			++i;

		_advance(xs, i);
	}
	return xs;
}

inline xstr_t *xstr_rstrip_bset(xstr_t *xs, const bset_t *rubbish)
{
	while (xs->len && BSET_TEST(rubbish, xs->data[xs->len-1]))
	{
		xs->len--;
	}
	return xs;
}

xstr_t *xstr_strip_bset(xstr_t *xs, const bset_t *rubbish)
{
	xstr_rstrip_bset(xs, rubbish);
	return xstr_lstrip_bset(xs, rubbish);
}

xstr_t *xstr_strip_char(xstr_t *xs, char rubbish)
{
	xstr_rstrip_char(xs, rubbish);
	return xstr_lstrip_char(xs, rubbish);
}

inline xstr_t *xstr_ltrim(xstr_t *xs)
{
	if (xs->len && ISSPACE(xs->data[0]))
	{
		size_t i = 1;
		while (i < xs->len && ISSPACE(xs->data[i]))
			++i;

		_advance(xs, i);
	}
	return xs;
}

inline xstr_t *xstr_rtrim(xstr_t *xs)
{
	while (xs->len && ISSPACE(xs->data[xs->len-1]))
	{
		xs->len--;
	}
	return xs;
}

inline xstr_t *xstr_trim(xstr_t *xs)
{
	xstr_rtrim(xs);
	return xstr_ltrim(xs);
}

xstr_t *xstr_lower(xstr_t *xs)
{
	size_t i;
	for (i = xs->len; i--; )
	{
		int ch = (uint8_t)xs->data[i];
		if (ch >= 'A' && ch <= 'Z')
			xs->data[i] = ch | 0x20;
	}
	return xs;
}

xstr_t *xstr_upper(xstr_t *xs)
{
	size_t i;
	for (i = xs->len; i--; )
	{
		int ch = (uint8_t)xs->data[i];
		if (ch >= 'a' && ch <= 'z')
			xs->data[i] = ch & ~0x20;
	}
	return xs;
}

size_t xstr_replace_char(xstr_t *xs, char needle, char replace)
{
	size_t i = xs->len;
	size_t n = 0;
	while (i)
	{
		--i;
		if (xs->data[i] == needle)
		{
			xs->data[i] = replace;
			++n;
		}
	}
	return n;
}

size_t xstr_replace_in(xstr_t *xs, const xstr_t *chset, const xstr_t *replace)
{
	size_t n = 0;
	ssize_t i = (ssize_t)xs->len;

	while (i-- > 0)
	{
		ssize_t k;
		uint8_t ch = xs->data[i];
		for (k = 0; k < (ssize_t)chset->len; ++k)
		{
			if (ch == chset->data[k])
			{
				if ((ssize_t)replace->len > 0)
				{
					if (k >= replace->len)
						k = replace->len - 1;
					xs->data[i] = replace->data[k];
				}
				++n;
				break;
			}
		}
	}

	return n;
}

size_t xstr_replace_in_cstr(xstr_t *xs, const char *chset, const char *replace)
{
	size_t n = 0;
	ssize_t i = (ssize_t)xs->len;
	ssize_t rlen = strlen(replace);

	while (i-- > 0)
	{
		uint8_t *p;
		uint8_t ch = xs->data[i];
		for (p = (uint8_t *)chset; *p; ++p)
		{
			if (ch == *p)
			{
				if (replace[0])
				{
					ssize_t k = p - (uint8_t *)chset;
					if (k > rlen)
						k = rlen - 1;
					xs->data[i] = replace[k];
				}
				++n;
				break;
			}
		}
	}

	return n;
}

size_t xstr_translate(xstr_t *xs, const uint8_t table[256])
{
	size_t n = 0;
	ssize_t i = (ssize_t)xs->len;

	while (i-- > 0)
	{
		uint8_t ch = xs->data[i];
		uint8_t target = table[(uint8_t)ch];
		if (ch != target)
		{
			xs->data[i] = target;
			++n;
		}
	}

	return n;
}

size_t xstr_count_char(const xstr_t *xs, char needle)
{
	size_t i = xs->len;
	size_t n = 0;
	while (i)
	{
		--i;
		if (xs->data[i] == (uint8_t)needle)
			++n;
	}
	return n;
}

size_t xstr_count_mem(const xstr_t *xs, const void *needle, size_t n)
{
	size_t k = 0;

	if (n)
	{
		const uint8_t *s = xs->data;
		size_t len = xs->len;
		const uint8_t *p;

		while ((p = (uint8_t *)_memmem(s, len, needle, n)) != NULL)
		{
			ssize_t m = p + n - s;
			s += m;
			len -= m;
			++k;
		}
	}
	return k;
}

size_t xstr_count(const xstr_t *xs, const xstr_t *needle)
{
	return xstr_count_mem(xs, needle->data, needle->len);
}

size_t xstr_count_cstr(const xstr_t *xs, const char *needle)
{
	return xstr_count_mem(xs, needle, strlen(needle));
}

size_t xstr_count_in_mem(const xstr_t *xs, const void *chset, size_t n)
{
	size_t k = 0;
	uint8_t *p = xs->data;
	uint8_t *end = xs->data + xs->len;

	if (n == 0)
	{
		return 0;
	}

	if (n == 1)
	{
		while (p < end && (p = (uint8_t *)memchr(p, *(uint8_t *)chset, end - p)) != NULL)
		{
			++p;
			++k;
		}
	}
	else
	{
		while (p < end && (p = (uint8_t *)_seek_in_mem(p, end - p, chset, n)) != NULL)
		{
			++p;
			++k;
		}
	}

	return k;
}

size_t xstr_count_in(const xstr_t *xs, const xstr_t *chset)
{
	return xstr_count_in_mem(xs, chset->data, chset->len);
}

size_t xstr_count_in_cstr(const xstr_t *xs, const char *chset)
{
	size_t k = 0;
	uint8_t *p = xs->data;
	uint8_t *end = xs->data + xs->len;

	if (chset[0] == 0)
	{
		return 0;
	}
	else if (chset[1] == 0)
	{
		while (p < end && (p = (uint8_t *)memchr(p, *(uint8_t *)chset, end - p)) != NULL)
		{
			++p;
			++k;
		}
	}
	else
	{
		while (p < end && (p = (uint8_t *)_seek_in_cstr(p, end - p, chset)) != NULL)
		{
			++p;
			++k;
		}
	}

	return k;
}

size_t xstr_count_in_bset(const xstr_t *xs, const bset_t *bset)
{
	size_t n = 0;
	ssize_t i = (ssize_t)xs->len;

	while (i-- > 0)
	{
		if (BSET_TEST(bset, xs->data[i]))
			++n;
	}
	return n;
}

bool xstr_delimit_mem(xstr_t *xs, const void *delimiter, size_t n, xstr_t *result)
{
	if (xs->data)
	{
		uint8_t *p, *s = xs->data;
		size_t k;

		if (xs->len && n && (p = (uint8_t *)_memmem(xs->data, xs->len, delimiter, n)) != NULL)
		{
			k = p - xs->data;
			_advance(xs, k + n);
		}
		else
		{
			k = xs->len;
			*xs = xstr_null;
		}

		if (result)
		{
			result->data = s;
			result->len = k;
		}
		return true;
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_delimit(xstr_t *xs, const xstr_t *delimiter, xstr_t *result)
{
	return xstr_delimit_mem(xs, delimiter->data, delimiter->len, result);
}

bool xstr_delimit_cstr(xstr_t *xs, const char *delimiter, xstr_t *result)
{
	return xstr_delimit_mem(xs, delimiter, strlen(delimiter), result);
}

bool xstr_delimit_char(xstr_t *xs, char delimiter, xstr_t *result)
{
	if (xs->data)
	{
		uint8_t *p, *s = xs->data;
		size_t k;

		if (xs->len && (p = (uint8_t *)memchr(xs->data, (uint8_t)delimiter, xs->len)) != NULL)
		{
			k = p - xs->data;
			_advance(xs, k + 1);
		}
		else
		{
			k = xs->len;
			*xs = xstr_null;
		}

		if (result)
		{
			result->data = s;
			result->len = k;
		}
		return true;
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

inline bool xstr_delimit_in_mem(xstr_t *xs, const void *chset, size_t n, xstr_t *result)
{
	if (xs->data)
	{
		uint8_t *p, *s = xs->data;
		size_t k;

		if (xs->len && (p = (uint8_t *)_seek_in_mem(xs->data, xs->len, chset, n)) != NULL)
		{
			k = p - xs->data;
			_advance(xs, k + 1);
		}
		else
		{
			k = xs->len;
			*xs = xstr_null;
		}

		if (result)
		{
			result->data = s;
			result->len = k;
		}
		return true;
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_delimit_in(xstr_t *xs, const xstr_t *chset, xstr_t *result)
{
	return xstr_delimit_in_mem(xs, chset->data, chset->len, result);
}

bool xstr_delimit_in_cstr(xstr_t *xs, const char *chset, xstr_t *result)
{
	if (xs->data)
	{
		uint8_t *p, *s = xs->data;
		size_t k;

		if (xs->len && (p = (uint8_t *)_seek_in_cstr(xs->data, xs->len, chset)) != NULL)
		{
			k = p - xs->data;
			_advance(xs, k + 1);
		}
		else
		{
			k = xs->len;
			*xs = xstr_null;
		}

		if (result)
		{
			result->data = s;
			result->len = k;
		}
		return true;
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_delimit_in_bset(xstr_t *xs, const bset_t *bset, xstr_t *result)
{
	if (xs->data)
	{
		uint8_t *s = xs->data;
		size_t k = 0;
		if (xs->len)
		{
			while (k < xs->len && !BSET_TEST(bset, xs->data[k]))
				++k;

			_advance(xs, k < xs->len ? k + 1 : k);
		}
		else
		{
			*xs = xstr_null;
		}

		if (result)
		{
			result->data = s;
			result->len = k;
		}
		return true;
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_delimit_in_space(xstr_t *xs, xstr_t *result)
{
	return xstr_delimit_in_bset(xs, &space_bset, result);
}

bool xstr_token_mem(xstr_t *xs, const void *chset, size_t n, xstr_t *result)
{
	if (xs->data && xs->len)
	{
		uint8_t *p = _seek_not_in_mem(xs->data, xs->len, chset, n);
		if (p)
		{
			size_t k = p - xs->data;
			_advance(xs, k);
			return xstr_delimit_in_mem(xs, chset, n, result);
		}
		else
		{
			*xs = xstr_null;
		}
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_token(xstr_t *xs, const xstr_t *chset, xstr_t *result)
{
	return xstr_token_mem(xs, chset->data, chset->len, result);
}

bool xstr_token_cstr(xstr_t *xs, const char *chset, xstr_t *result)
{
	if (xs->data && xs->len)
	{
		uint8_t *p = _seek_not_in_cstr(xs->data, xs->len, chset);
		if (p)
		{
			size_t k = p - xs->data;
			_advance(xs, k);
			return xstr_delimit_in_cstr(xs, chset, result);
		}
		else
		{
			*xs = xstr_null;
		}
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_token_bset(xstr_t *xs, const bset_t *bset, xstr_t *result)
{
	if (xs->data && xs->len)
	{
		size_t k = 0;
		while (k < xs->len && BSET_TEST(bset, xs->data[k]))
			++k;

		if (k < xs->len)
		{
			size_t m = k + 1;
			while (m < xs->len && !BSET_TEST(bset, xs->data[m]))
				++m;

			if (result)
			{
				result->data = xs->data + k;
				result->len = m - k;
			}
			_advance(xs, m < xs->len ? m + 1 : m);
			return true;
		}
		else
		{
			*xs = xstr_null;
		}
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_token_char(xstr_t *xs, char rubbish, xstr_t *result/*NULL*/)
{
	if (xs->data && xs->len)
	{
		ssize_t k = 0;
		while (k < xs->len && xs->data[k] == rubbish)
			++k;

		if (k < xs->len)
		{
			ssize_t m = k + 1;
			while (m < xs->len && xs->data[m] != rubbish)
				++m;

			if (result)
			{
				result->data = xs->data + k;
				result->len = m - k;
			}
			_advance(xs, m < xs->len ? m + 1 : m);
			return true;
		}
		else
		{
			*xs = xstr_null;
		}
	}

	if (result)
	{
		*result = xstr_null;
	}
	return false;
}

bool xstr_token_space(xstr_t *xs, xstr_t *result)
{
	return xstr_token_bset(xs, &space_bset, result);
}

ssize_t xstr_key_value(const xstr_t *xs, char delimiter, xstr_t *key/*NULL*/, xstr_t *value/*NULL*/)
{
	uint8_t *p;

	if (xs->len && (p = (uint8_t *)memchr(xs->data, (uint8_t)delimiter, xs->len)) != NULL)
	{
		if (key)
		{
			xstr_init(key, xs->data, p - xs->data);
			xstr_trim(key);
		}

		if (value)
		{
			uint8_t *v = p + 1;
			xstr_init(value, v, xs->data + xs->len - v);
			xstr_trim(value);
		}
		return p - xs->data;
	}

	if (key)
	{
		*key = xstr_null;
	}

	if (value)
	{
		*value = *xs;
		xstr_trim(value);
	}

	return -1;
}

void make_translate_table_from_xstr(uint8_t table[256], const xstr_t *chset, const xstr_t *replace)
{
	ssize_t k;

	for (k = 0; k < 256; ++k)
	{
		table[k] = k;
	}

	if ((ssize_t)replace->len > 0)
	{
		k = (ssize_t)chset->len;
		while (k-- > 0)
		{
			int ch = (uint8_t)chset->data[k];
			ssize_t x = (k < replace->len) ? k : replace->len - 1;
			table[ch] = replace->data[x];
		}
	}
}

void make_translate_table_from_cstr(uint8_t table[256], const char *chset, const char *replace)
{
	ssize_t k;

	for (k = 0; k < 256; ++k)
	{
		table[k] = k;
	}

	if (replace[0])
	{
		ssize_t rlen = strlen(replace);
		k = strlen(chset);
		while (k-- > 0)
		{
			int ch = (uint8_t)chset[k];
			ssize_t x = (k < rlen) ? k : rlen - 1;
			table[ch] = replace[x];
		}
	}
}



#define DEF(TYPE, NAME)		const TYPE NAME[] =				\
{										\
	F(2), F(3), F(4), F(5), F(6), F(7), F(8), F(9), F(10),			\
	F(11), F(12), F(13), F(14), F(15), F(16), F(17), F(18), F(19), F(20),	\
	F(21), F(22), F(23), F(24), F(25), F(26), F(27), F(28), F(29), F(30),	\
	F(31), F(32), F(33), F(34), F(35), F(36)				\
}

#define F(X)   UINTMAX_MAX / X
static DEF (uintmax_t, _uintmax_max_tab);
#undef F

#define F(X)   UINTMAX_MAX % X
static DEF (uint8_t, _uintmax_rem_tab);
#undef F


static bool _a2uintmax(const xstr_t *xs, xstr_t *end, int base, bool *p_negative, uintmax_t *p_result)
{
	uintmax_t result = 0;
	uintmax_t cutoff;
	int cutlim;
	bool overflow = false;
	uint8_t *s, *invalid, *last;
	int n, ch;

	s = (uint8_t *)xs->data;
	invalid = s;
	last = s + xs->len;

	*p_negative = false;

	while (s < last && ISSPACE(*s))
		++s;

	if (s >= last)
		goto done;

	if (*s == '-')
	{
		*p_negative = true;
		if (++s >= last)
			goto done;
	}
	else if (*s == '+')
	{
		if (++s >= last)
			goto done;
	}

	if (base < 0 || base == 1 || base > 36)
		base = 0;

	if (*s == '0')
	{
		if (s + 1 < last && (s[1] == 'x' || s[1] == 'X'))
		{
			if (base == 0 || base == 16)
			{
				base = 16;
				s += 2;
			}
		}
		else if (base == 0)
			base = 8;
	}
	else if (base == 0)
		base = 10;

	cutoff = _uintmax_max_tab[base-2];
	cutlim = _uintmax_rem_tab[base-2];

	for (; s < last; ++s)
	{
		ch = (uint8_t)*s;
		if (ch >= '0' && ch <= '9')
			n = (uint8_t)(ch - '0');
		else
			n = (uint8_t)((ch | 0x20) - 'a') + 10;

		if (n >= base)
			break;

		invalid = s + 1;
		if (result > cutoff || (result == cutoff && n > cutlim))
		{
			overflow = true;
		}
		else
		{
			result *= base;
			result += n;
		}
	}

done:
	if (end)
		xstr_init(end, (uint8_t *)invalid, last - invalid);
	*p_result = result;
	return overflow;
}

inline long xstr_to_long(const xstr_t *xs, xstr_t *end, int base)
{
	uintmax_t r;
	long result;
	bool negative;
	bool overflow = _a2uintmax(xs, end, base, &negative, &r);
	if (negative)
	{
		if (overflow || r > (unsigned long)LONG_MIN)
		{
			errno = ERANGE;
			result = LONG_MIN;
		}
		else
			result = -(intmax_t)r;
	}
	else 
	{
		if (overflow || r > LONG_MAX)
		{
			errno = ERANGE;
			result = LONG_MAX;
		}
		else
			result = r;
	}
	return result;
}

inline long long xstr_to_llong(const xstr_t *xs, xstr_t *end, int base)
{
	uintmax_t r;
	long long result;
	bool negative;
	bool overflow = _a2uintmax(xs, end, base, &negative, &r);
	if (negative)
	{
		if (overflow || r > (unsigned long long)LLONG_MIN)
		{
			errno = ERANGE;
			result = LLONG_MIN;
		}
		else
			result = -(intmax_t)r;
	}
	else
	{
		if (overflow || r > LLONG_MAX)
		{
			errno = ERANGE;
			result = LLONG_MAX;
		}
		else
			result = r;
	}
	return result;
}

intmax_t xstr_to_integer(const xstr_t *xs, xstr_t *end, int base)
{
	uintmax_t r;
	intmax_t result;
	bool negative;
	bool overflow = _a2uintmax(xs, end, base, &negative, &r);
	if (negative)
	{
		if (overflow || r > (unsigned long long)INTMAX_MIN)
		{
			errno = ERANGE;
			result = INTMAX_MIN;
		}
		else
			result = -(intmax_t)r;
	}
	else
	{
		if (overflow || r > INTMAX_MAX)
		{
			errno = ERANGE;
			result = INTMAX_MAX;
		}
		else
			result = r;
	}
	return result;
}


unsigned long xstr_to_ulong(const xstr_t *xs, xstr_t *end, int base)
{
	uintmax_t r;
	bool negative;
	bool overflow = _a2uintmax(xs, end, base, &negative, &r);
	if (overflow || r > ULONG_MAX)
	{
		errno = ERANGE;
		return ULONG_MAX;
	}
	return negative ? -(intmax_t)r : r;
}

unsigned long long xstr_to_ullong(const xstr_t *xs, xstr_t *end, int base)
{
	uintmax_t r;
	bool negative;
	bool overflow = _a2uintmax(xs, end, base, &negative, &r);
	if (overflow || r > ULLONG_MAX)
	{
		errno = ERANGE;
		return ULLONG_MAX;
	}
	return negative ? -(intmax_t)r : r;
}

uintmax_t xstr_to_uinteger(const xstr_t *xs, xstr_t *end, int base)
{
	uintmax_t r;
	bool negative;
	bool overflow = _a2uintmax(xs, end, base, &negative, &r);
	if (overflow)
	{
		errno = ERANGE;
		return UINTMAX_MAX;
	}
	return negative ? -(intmax_t)r : r;
}


int xstr_atoi(const xstr_t *str)
{
	return xstr_to_long(str, NULL, 10);
}

long xstr_atol(const xstr_t *str)
{
	return xstr_to_long(str, NULL, 10);
}

long long xstr_atoll(const xstr_t *str)
{
	return xstr_to_llong(str, NULL, 10);
}

unsigned long long xstr_atoull(const xstr_t *str)
{
	return xstr_to_ullong(str, NULL, 10);
}


static double _a2d(const xstr_t *xs, xstr_t *end)
{
	long double value = 0.0;
	long double factor = 1.0;
	unsigned long expo = 0;
	bool hex = false;
	bool expo_negative = false;
	bool negative = false;
	bool has_fraction = false;
	uint8_t *s, *invalid, *last;
	unsigned int n;
	enum
	{
		BEFORE_POINT,
		AFTER_POINT,
		EXPONENT_MINUS,
		EXPONENT_DIGIT,
	} state = BEFORE_POINT;

	s = (uint8_t *)xs->data;
	invalid = s;
	last = s + xs->len;

	while (s < last && ISSPACE(*s))
		++s;

	if (s >= last)
		goto done;

	if (*s == '-')
	{
		negative = true;
		if (++s >= last)
			goto done;
	}
	else if (*s == '+')
	{
		if (++s >= last)
			goto done;
	}

	if (*s == '0')
	{
		if (s + 1 < last && (s[1] == 'x' || s[1] == 'X'))
		{
			hex = true;
			s += 2;
		}
	}
	else if (*s == 'i' || *s == 'I')
	{
		xstr_t str = XSTR_INIT((uint8_t *)s, last - s);
		if (xstr_case_start_with_cstr(&str, "inf"))
		{
			value = HUGE_VAL;
			if (xstr_case_start_with_cstr(&str, "infinity"))
				invalid = s + 8;
			else
				invalid = s + 3;
		}
		goto done;
	}
	else if (*s == 'n' || *s == 'N')
	{
		xstr_t str = XSTR_INIT((uint8_t *)s, last - s);
		if (xstr_case_start_with_cstr(&str, "nan"))
		{
			value = NAN;
			s += 3;
			invalid = s;
			if (s < last && *s == '(')
			{
				while (++s < last && isalnum(*s))
					continue;

				if (s < last && *s == ')')
					invalid = s + 1;
			}
		}
		goto done;
	}

	for (; s < last; ++s)
	{
		switch (*s)
		{
		case 'e':
		case 'E':
			if (!hex)
				goto handle_exponent;

		case 'a': case 'b': case 'c': case 'd': case 'f':
		case 'A': case 'B': case 'C': case 'D': case 'F':
			if (hex)
			{
				n = (*s | 0x20) - 'a' + 10;
			}
			else
			{
				goto done;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
				n = (unsigned int)(*s - '0');
			}

			if (state == BEFORE_POINT)
			{
				has_fraction = true;
				value = value * (hex ? 16 : 10) + n;
			}
			else if (state == AFTER_POINT)
			{
				has_fraction = true;
				factor *= hex ? 1.0 / 16 : 0.1;
				value += n * factor;
			}
			else
			{
				if (state == EXPONENT_MINUS)
					state = EXPONENT_DIGIT;

				if (expo < ULONG_MAX / 10)	// overflow
					expo = expo * 10 + n;
			}
			invalid = s + 1;
			break;

		case '.':
			if (state != BEFORE_POINT)
				goto done;
			state = AFTER_POINT;
			if (has_fraction)
				invalid = s + 1;
			break;

		case 'p':
		case 'P':
			if (!hex)
				goto done;
		handle_exponent:
			if (state >= EXPONENT_MINUS || !has_fraction)
				goto done;
			state = EXPONENT_MINUS;
			break;

		case '-':
			expo_negative = true;
		case '+':
			if (state != EXPONENT_MINUS)
				goto done;
			state = EXPONENT_DIGIT;
			break;

		default:
			goto done;
		}
	}

done:
	if (expo)
	{
		if (hex)
			factor = expo_negative ? 0.5 : 2.0;
		else
			factor = expo_negative ? 0.1 : 10.0;

		do
		{
			if (expo & 0x01)
				value *= factor;
			factor *= factor;
		} while ((expo >>= 1) != 0);
	}

	if (end)
		xstr_init(end, (uint8_t *)invalid, last - invalid);

	if (invalid == (uint8_t *)xs->data)
		return 0.0;

	return negative ? -value : value;
}

double xstr_to_double(const xstr_t *str, xstr_t *end/*NULL*/)
{
	return _a2d(str, end);
}

