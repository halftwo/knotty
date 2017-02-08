#include "xutf8.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xutf8.c,v 1.2 2015/06/15 07:36:33 gremlin Exp $";
#endif


#define GET_2BYTES(pcode, s, c0)	do {		\
	int c1 = (unsigned char)s[1];			\
	if (c1 < 0x80 || c1 >= 0xc0) goto error;	\
	/* code < 0x80 and not modified utf8 0x00 */	\
	if (c0 < 0xc2 && !(c0 == 0xc0 && c1 == 0x80)) goto error; \
	*pcode = ((c0 & 0x1f) << 6) | (c1 & 0x3f);	\
} while (0)


#define GET_3BYTES(pcode, s, c0)	do {		\
	int c1, c2;					\
	c1 = (unsigned char)s[1];			\
	if (c1 < 0x80 || c1 >= 0xc0) goto error;	\
	/* code < 0x800 */				\
	if (c0 == 0xe0 && c1 < 0xa0) goto error;	\
	c2 = (unsigned char)s[2];			\
	if (c2 < 0x80 || c2 >= 0xc0) goto error;	\
	*pcode = ((c0 & 0x0f) << 12) | ((c1 & 0x3f) << 6) | (c2 & 0x3f); \
} while (0)


#define GET_4BYTES(pcde, s, c0)		do {		\
	int c1, c2, c3;					\
	c1 = (unsigned char)s[1];			\
	if (c1 < 0x80 || c1 >= 0xc0) goto error;	\
	/* code  < 0x010000 */				\
	if (c0 == 0xf0 && c1 < 0x90) goto error;	\
	/* code >= 0x110000 */				\
	if (c0 == 0xf4 && c1 >= 0x90) goto error;	\
	c2 = (unsigned char)s[2];			\
	if (c2 < 0x80 || c2 >= 0xc0) goto error;	\
	c3 = (unsigned char)s[3];			\
	if (c3 < 0x80 || c3 >= 0xc0) goto error;	\
	*pcode = ((c0 & 0x07) << 18) | ((c1 & 0x3f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f); \
} while (0)


static inline bool get_surrogate_second(int *pcode, unsigned char *s, int high)
{
	int low;
	int c = s[0];
	if (c != 0xed)
		return false;

	GET_3BYTES(&low, s, c);
	if (!(low >= 0xdc00 && low <= 0xdfff))
		return false;

	high -= 0xd800;
	low -= 0xdc00;
	*pcode = ((high << 10) + low) + 0x10000;
	return true;
error:
	return false;
}


size_t xutf8_cstr_to_code(const char *s, int *pcode)
{
	int c0 = (unsigned char)*s;

	if (c0 < 0x80)
	{
		if (c0 == 0)
			goto error;

		*pcode = c0;
		return 1;
	}
	else if (c0 <= 0xdf)
	{
		GET_2BYTES(pcode, s, c0);
		return 2;
	}
	else if (c0 <= 0xef)
	{
		GET_3BYTES(pcode, s, c0);
		if (*pcode >= 0xd800 && *pcode <= 0xdfff)
		{
			int high = *pcode;
			if (high >= 0xdc00)
				goto error;

			if (!get_surrogate_second(pcode, (unsigned char *)s + 3, high))
				goto error;
			return 6;
		}
		return 3;
	}
	else if (c0 <= 0xf4)
	{
		GET_4BYTES(pcode, s, c0);
		return 4;
	}

error:
	*pcode = -1;
	return 0;
}

size_t xutf8_xstr_to_code(const xstr_t *xs, int *pcode)
{
	int c0;
	unsigned char *s = xs->data;

	if (xs->len < 1)
		goto error;

	c0 = s[0];
	if (c0 < 0x80)
	{
		*pcode = c0;
		return 1;
	}
	else if (c0 <= 0xdf)
	{
		if (xs->len < 2)
			goto error;

		GET_2BYTES(pcode, s, c0);
		return 2;
	}
	else if (c0 <= 0xef)
	{
		if (xs->len < 3)
			goto error;

		GET_3BYTES(pcode, s, c0);
		if (*pcode >= 0xd800 && *pcode <= 0xdfff)
		{
			int high = *pcode;
			if (high >= 0xdc00)
				goto error;

			if (xs->len < 6)
				goto error;

			if (!get_surrogate_second(pcode, (unsigned char *)s + 3, high))
				goto error;
			return 6;
		}
		return 3;
	}
	else if (c0 <= 0xf4)
	{
		if (xs->len < 4)
			goto error;

		GET_4BYTES(pcode, s, c0);
		return 4;
	}

error:
	*pcode = -1;
	return 0;
}


size_t xutf8_cstr_from_code(char *buf, int c)
{
	char *p = buf;

	if (c <= 0x7F)
	{
		if (c >= 0)
			*p++ = c;
	}
	else if (c <= 0x7FF)
	{
		*p++ = (c >> 6) | 0xc0;
		*p++ = (c & 0x3f) | 0x80;
	}
	else if (c <= 0xFFFF)
	{
		*p++ = (c >> 12) | 0xe0;
		*p++ = ((c >> 6) & 0x3f) | 0x80;
		*p++ = (c & 0x3f) | 0x80;
	}
	else if (c <= 0x10FFFF)
	{
		*p++ = (c >> 18) | 0xf0;
		*p++ = ((c >> 12) & 0x3f) | 0x80;
		*p++ = ((c >> 6) & 0x3f) | 0x80;
		*p++ = (c & 0x3f) | 0x80;
	}

	*p = 0;
	return p - buf;
}


size_t xutf8_cstr_count(const char *s, char **end)
{
	size_t count = 0;

	while (s[0])
	{
		int code;
		size_t n = xutf8_cstr_to_code(s, &code);
		if (n == 0)
			break;
		s += n;
		++count;
	}

	if (end)
		*end = (char *)s;
	return count;
}

size_t xutf8_xstr_count(const xstr_t *xs, xstr_t *end)
{
	size_t count = 0;
	xstr_t s = *xs;

	while (s.len)
	{
		int code;
		size_t n = xutf8_xstr_to_code(&s, &code);
		if (n == 0)
			break;
		s.data += n;
		s.len -= n;
		++count;
	}

	if (end)
		*end = s;
	return count;
}


#ifdef TEST_XUTF8

#include <assert.h>

int main(int argc, char **argv)
{
	/* with utf16 surrogate code point */
	const char *utf8 = "\xed\xa0\xb4\xed\xb4\x9e \xe4\xbd\xa0\xe5\xa5\xbd";
	char *end;

	// without utf16 surrogate code point */
	xstr_t utf8_xs = XSTR_CONST("\xf0\x9d\x84\x9e \xe4\xbd\xa0\xe5\xa5\xbd");
	xstr_t end_xs;
	int n;

	n  = xutf8_cstr_count(utf8, &end);
	assert(n == 4 && *end == 0);

	n = xutf8_xstr_count(&utf8_xs, &end_xs);
	assert(n == 4 && end_xs.len == 0);

	return 0;
}

#endif

