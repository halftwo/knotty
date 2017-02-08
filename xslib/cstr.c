/*
   Author: XIONG Jiagui
   Date: 2007-03-08
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "cstr.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: cstr.c,v 1.30 2012/09/20 03:21:47 jiagui Exp $";
#endif


inline size_t cstr_from_ulong(char buf[], unsigned long n)
{
	char *p = buf;
	size_t len;

	do
	{
		*p++ = "0123456789"[n % 10];
	} while ((n /= 10) != 0);
	*p = 0;
	len = p - buf;

	for (--p; p > buf; --p, ++buf)
	{
		char c = *p;
		*p = *buf;
		*buf = c;
	}
	return len;
}

inline size_t cstr_from_ullong(char buf[], unsigned long long n)
{
	char *p = buf;
	size_t len;

	do
	{
		*p++ = "0123456789"[n % 10];
	} while ((n /= 10) != 0);
	*p = 0;
	len = p - buf;

	for (--p; p > buf; --p, ++buf)
	{
		char c = *p;
		*p = *buf;
		*buf = c;
	}
	return len;
}

size_t cstr_from_long(char buf[], long v)
{
	long long n;
	int sign;

	if (v >= 0)
	{
		sign = 0;
		n = v;
	}
	else
	{
		sign = 1;
		n = -v;
		*buf++ = '-';
	}
	return sign + cstr_from_ulong(buf, n);
}

size_t cstr_from_llong(char buf[], long long v)
{
	unsigned long long n;
	int sign;

	if (v >= 0)
	{
		sign = 0;
		n = v;
	}
	else
	{
		sign = 1;
		n = -v;
		*buf++ = '-';
	}
	return sign + cstr_from_ullong(buf, n);
}

inline size_t cstr_nlen(const char *str, size_t max)
{
	const char *p = (char *)memchr(str, 0, max);
	return p ? p - str : max;
}

char *cstr_ndup(const char *src, size_t max)
{
	size_t n = cstr_nlen(src, max);
	char *p = malloc(n + 1);
	if (p)
	{
		memcpy(p, src, n);
		p[n] = 0;
	}
	return p;
	
}

void *cmem_dup(const void *src, size_t n)
{
	void *p = malloc(n ? n : 1);
	if (p && n)
	{
		memcpy(p, src, n);
	}
	return p;
}


char *cstr_lstrip(const char *str, const char *rubbish)
{
	if (rubbish == NULL || rubbish[0] == 0)
	{
		int c;
		for (; (c = *(unsigned char *)str++) != 0; )
		{
			if (!isspace(c))
				break;
		}
		return (char *)--str;
	}

	return (char *)str + strspn(str, rubbish);
}

char *cstr_rstrip(char *str, const char *rubbish)
{
	int len;
	char *p;

	len = strlen(str);
	p = &str[len];
	if (rubbish == NULL || rubbish[0] == 0)
	{
		for (--p; p >= str; --p)
		{
			if (!isspace(*(unsigned char *)p))
				break;
		}
		*++p = 0;
	}
	else
	{
		for (--p; p >= str; --p)
		{
			if (!strchr(rubbish, *p))
				break;
		}
		*++p = 0;
	}
	return p;
}

char *cstr_strip(char *str, const char *rubbish)
{
	str = cstr_lstrip(str, rubbish);
	if (str[0])
		cstr_rstrip(str, rubbish);
	return str;
}

char *cstr_trim(char *str)
{
	str = cstr_lstrip(str, NULL);
	if (str[0])
		cstr_rstrip(str, NULL);
	return str;
}

char *cstr_lower(char *s)
{
	for (; *s; ++s)
		*s = tolower(*(unsigned char *)s);
	return s;
}

char *cstr_upper(char *s)
{
	for (; *s; ++s)
		*s = toupper(*(unsigned char *)s);
	return s;
}

size_t cstr_tolower(char *dst, const char *str)
{
	char *d = dst ? dst : (char *)str;
	for (; *str; ++str)
		*d++ = tolower(*(unsigned char *)str);
	return d - dst;
}

size_t cstr_toupper(char *dst, const char *str)
{
	char *d = dst ? dst : (char *)str;
	for (; *str; ++str)
		*d++ = toupper(*(unsigned char *)str);
	return d - dst;
}

bool cstr_isalpha(const char *str)
{
	int c = *(unsigned char *)str++;
	if (!isalpha(c))
		return false;
	for (; (c = *(unsigned char *)str++); )
		if (!isalpha(c))
			return false;
	return true;
}

bool cstr_isalnum(const char *str)
{
	int c = *(unsigned char *)str++;
	if (!isalnum(c))
		return false;
	for (; (c = *(unsigned char *)str++); )
		if (!isalnum(c))
			return false;
	return true;
}

bool cstr_isdigit(const char *str)
{
	int c = *(unsigned char *)str++;
	if (!isdigit(c))
		return false;
	for (; (c = *(unsigned char *)str++); )
		if (!isdigit(c))
			return false;
	return true;
}

bool cstr_isxdigit(const char *str)
{
	int c = *(unsigned char *)str++;
	if (!isxdigit(c))
		return false;
	for (; (c = *(unsigned char *)str++); )
		if (!isxdigit(c))
			return false;
	return true;
}

bool cstr_isupper(const char *str)
{
	int c = *(unsigned char *)str++;
	if (!isupper(c))
		return false;
	for (; (c = *(unsigned char *)str++); )
		if (!isupper(c))
			return false;
	return true;
}

bool cstr_islower(const char *str)
{
	int c = *(unsigned char *)str++;
	if (!islower(c))
		return false;
	for (; (c = *(unsigned char *)str++); )
		if (!islower(c))
			return false;
	return true;
}

bool cstr_isspace(const char *str)
{
	int c = *(unsigned char *)str++;
	if (!isspace(c))
		return false;
	for (; (c = *(unsigned char *)str++); )
		if (!isspace(c))
			return false;
	return true;
}

bool cstr_start_with(const char *str, const char *prefix)
{
	while (*prefix && *prefix == *str)
	{
		++prefix;
		++str;
	}

	return (*prefix == 0);
}

bool cstr_end_with(const char *str, const char *suffix)
{
	int n;
	int len = strlen(suffix);

	if (len == 0)
		return true;
	
	n = strlen(str);
	if (n < len)
		return false;
	
	return (memcmp(&str[n - len], suffix, len) == 0);
}

size_t cstr_count(const char *str, char needle)
{
	size_t n = 0;
	const char *p;
	for (p = str; *p; ++p)
	{
		if (*p == needle)
			++n;
	}
	return n;
}

size_t cstr_translate(char *str, const unsigned char table[256])
{
	int c;
	char *s = str;

	for (; (c = *(unsigned char *)s) != 0; ++s)
	{
		int d = table[c];
		if (d != c)
			*s = d;
	}

	return s - str;
}


char *cstr_delimit(char **strptr, char delimiter)
{
	char *p;
	char *s = *strptr;
	if (s == NULL)
		return NULL;
	
	if (delimiter && (p = strchr(s, delimiter)) != NULL)
	{
		*p++ = 0;
		*strptr = p;
	}
	else
		*strptr = NULL;
	return s;
}

char *cstr_rchar(const char *str, const char *end, char ch)
{
	if (end)
	{
		if (end <= str)
			return NULL;
		return (char *)memrchr(str, ch, end - str);
	}

	return strrchr(str, ch);
}

char *cstr_rstr(const char *haystack, const char *end, const char *needle)
{
	int hlen, nlen;
	const char *p;

	if (end)
	{
		hlen = end - haystack;
		if (hlen < 0)
			return NULL;
	}
	else
	{
		hlen = strlen(haystack);
		end = haystack + hlen;
	}

	if (needle[0] == 0)
		return (char *)end;

	nlen = strlen(needle);
	if (nlen > hlen)
		return NULL;

	if (nlen == 1)
		return (char *)memrchr(haystack, *needle, hlen);
	
	for (p = end - nlen; p >= haystack; --p)
	{
		if (*p == *needle && strcmp(p + 1, needle + 1) == 0)
			return (char *)p;
	}

	return NULL;
}

char *cstr_pcopy(char *buf, const char *end, const char *src)
{
	if (*src)
	{
		--end;
		if (buf < end)
		{
			do
			{
				*buf++ = *src++;
			} while (*src && buf < end);

			*buf = 0;
			if (*src == 0)
			{
				return buf;
			}
		}

		buf += strlen(src);
	}

	return buf;
}

char *cstr_pcopyn(char *buf, const char *end, const char *src, size_t num)
{
	if ((ssize_t)num > 0)
	{
		--end;
		if (buf < end)
		{
			size_t size = end - buf;
			size_t n = (size < num) ? size : num;
			char *p = (char *)memccpy(buf, src, 0, n);
			if (p)
			{
				return p - 1;
			}

			buf += n;
			*buf = 0;

			if (size < num)
				buf += cstr_nlen(src + size, num - size);
		}
		else
		{
			buf += cstr_nlen(src, num);
		}
	}

	return buf;
}

char *cstr_pputc(char *buf, const char *end, char ch)
{
	if (buf < end - 1)
	{
		buf[0] = ch;
		buf[1] = 0;
	}
	return buf + 1;
}

size_t cstr_ncopy(char *buf, size_t size, const char *src)
{
	size_t n = 0;

	if (*src)
	{
		if (size-- > 1)
		{
			do
			{
				buf[n++] = *src++;
			} while (*src && n < size);

			buf[n] = 0;
			if (*src == 0)
			{
				return n;
			}
		}

		n += strlen(src);
	}

	return n;
}

size_t cstr_ncopyn(char *buf, size_t size, const char *src, size_t num)
{
	size_t n = 0;

	if ((ssize_t)num > 0)
	{
		if (size-- > 1)
		{
			char *p;
			n = size < num ? size : num;
			p = (char *)memccpy(buf, src, 0, n);
			if (p)
			{
				return p - 1 - buf;
			}

			buf[n] = 0;
		}

		n += cstr_nlen(src + n, num - n);
	}

	return n;
}

size_t cstr_nputc(char *buf, size_t size, char ch)
{
	if (size > 1)
	{
		buf[0] = ch;
		buf[1] = 0;
	}
	return 1;
}


size_t cstr_move(char *dst, const char *src)
{
	int len;
	if (dst <= src)
	{
		char *d = dst;
		for (; *src; )
			*d++ = *src++;
		*d = 0;
		len = d - dst;
	}
	else
	{
		len = strlen(src);
		memmove(dst, src, len + 1);
	}

	return len;
}

size_t cstr_replace(char *dst, const char *src, char match, char target)
{
	size_t n = 0;
	char *s, *d, *p;

	for (d = (char *)dst, s = (char *)src; ((p = (char *)strchr(s, match)) != NULL); s = p)
	{
		++n;
		if (dst)
		{
			memcpy(d, s, p - s);
			d += p - s;
			*d++ = target;
			p++;
		}
		else
			*p++ = target;
	}

	if (n > 0 && dst)
	{
		strcpy(d, s);
	}

	return n;
}

size_t cstr_nreplace(char *dst, const char *src, size_t size, char match, char target)
{
	size_t n = 0;
	char *s, *d, *p;
	size_t len = size;

	for (d = (char *)dst, s = (char *)src; ((p = (char *)memchr(s, match, len)) != NULL); len -= (p - s), s = p)
	{
		++n;
		if (dst)
		{
			memcpy(d, s, p - s);
			d += p - s;
			*d++ = target;
			p++;
		}
		else
			*p++ = target;
	}

	if (n > 0 && dst)
	{
		memcpy(d, s, len);
	}

	return n;
}


size_t cstr_backslash_escape(char *dst, const char *src, const char *meta, const char *subst)
{
	char *d = dst;
	unsigned char *s = (unsigned char *)src;
	size_t subst_size = subst ? strlen(subst) : 0;
	int c;
	const char *p;

	while ((c = *s++) != 0)
	{
		if ((p = strchr(meta, c)) != NULL)
		{
			*d++ = '\\';
			size_t idx = p - meta;
			if (idx < subst_size)
				*d++ = subst[idx];
			else
				*d++ = c;
		}
		else
		{
			*d++ = c;
		}
	}

	*d = 0;
	return d - dst;
}

char *cstr_copy(char *buf, const char *src)
{
	if (*src)
	{
		do
		{
			*buf++ = *src++;
		} while (*src);
		*buf = 0;
	}

	return buf;
}

void *cmem_copy(void *dest, const void *src, size_t n)
{
	return memcpy(dest, src, n) + n;
}

char *cstr_copyn(char *buf, const char *src, size_t num)
{
	if ((ssize_t)num > 0)
	{
		char *p = (char *)memccpy(buf, src, 0, num);
		if (p)
		{
			buf = p - 1;
		}
		else
		{
			buf += num;
			*buf = 0;
		}
	}
	return buf;
}

char *cstr_chrnul(const char *str, int ch)
{
#if __GNUC__
	return strchrnul(str, ch);
#else
	for (; *str; ++str)
	{
		if (*str == ch)
			break;
	}
	return (char *)str;
#endif
}

int cstr_casecmp(const char *s1, const char *s2)
{
	int c1, c2;

	do
	{
		c1 = (unsigned char)*s1++;
		c2 = (unsigned char)*s2++;

		if (c1 != c2)
		{
			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c2 >= 'A' && c2 <= 'Z')
				c2 |= 0x20;

			if (c1 != c2)
				return c1 - c2;
		}
	} while (c1);

	return 0;
}

int cstr_ncasecmp(const char *s1, const char *s2, size_t n)
{
	int c1, c2;

	if (n)
	{
		do
		{
			c1 = (unsigned char)*s1++;
			c2 = (unsigned char)*s2++;

			if (c1 != c2)
			{
				if (c1 >= 'A' && c1 <= 'Z')
					c1 |= 0x20;
				if (c2 >= 'A' && c2 <= 'Z')
					c2 |= 0x20;

				if (c1 != c2)
					return c1 - c2;
			}

			--n;
		} while (n && c1);
	};

	return 0;
}

char *cstr_strn(const char *haystack, const char *needle, size_t n)
{
	int c1, c2;

	c2 = (unsigned char)*needle++;
	if (n == 0 || c2 == 0)
		return (char *)haystack;

	--n;
	if (n == 0)
		return strchr(haystack, c2);

	do
	{
		do
		{
			c1 = (unsigned char)*haystack++;
			if (c1 == 0)
				return NULL;
		} while (c1 != c2);
	} while (strncmp(haystack, needle, n) != 0);

	return (char *)--haystack;
}

char *cstr_casestr(const char *haystack, const char *needle)
{
	int c1, c2;

	c2 = (unsigned char)*needle++;
	if (c2 == 0)
		return (char *)haystack;

	if (c2 >= 'A' && c2 <= 'Z')
		c2 |= 0x20;

	if (*needle == 0)
	{
		do
		{
			c1 = (unsigned char)*haystack++;
			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c1 == 0)
				return NULL;
		} while (c1 != c2);
	}
	else
	{
		do
		{
			do
			{
				c1 = (unsigned char)*haystack++;
				if (c1 == 0)
					return NULL;
				if (c1 >= 'A' && c1 <= 'Z')
					c1 |= 0x20;
			} while (c1 != c2);
		} while (cstr_casecmp(haystack, needle) != 0);
	}

	return (char *)--haystack;
}

char *cstr_casestrn(const char *haystack, const char *needle, size_t n)
{
	int c1, c2;

	c2 = (unsigned char)*needle++;
	if (n == 0 || c2 == 0)
		return (char *)haystack;

	if (c2 >= 'A' && c2 <= 'Z')
		c2 |= 0x20;

	--n;
	if (n == 0)
	{
		do
		{
			c1 = (unsigned char)*haystack++;
			if (c1 >= 'A' && c1 <= 'Z')
				c1 |= 0x20;
			if (c1 == 0)
				return NULL;
		} while (c1 != c2);
	}
	else
	{
		do
		{
			do
			{
				c1 = (unsigned char)*haystack++;
				if (c1 == 0)
					return NULL;
				if (c1 >= 'A' && c1 <= 'Z')
					c1 |= 0x20;
			} while (c1 != c2);
		} while (cstr_ncasecmp(haystack, needle, n) != 0);
	}

	return (char *)--haystack;
}

