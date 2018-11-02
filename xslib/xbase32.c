#include "xbase32.h"
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#define NI	-3
#define SP	-2

static int8_t detab[128] = {
	NI, -1, -1, -1, -1, -1, -1, -1, -1, SP, SP, SP, SP, SP, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	SP, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, 16, 17, -1, 18, 19, -1, 20, 21, -1,
	22, 23, 24, 25, 26, -1, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, 16, 17, -1, 18, 19, -1, 20, 21, -1,
	22, 23, 24, 25, 26, -1, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1,
};

const char xbase32_alphabet[] = "0123456789abcdefghjkmnpqrstvwxyz";
const char xbase32_Alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static ssize_t _encode(const char *alphabet, char *out, const void *in, ssize_t len)
{
	const unsigned char *s = (const unsigned char *)in;
	char *d = out;
	int c0, c1, c2, c3, c4;

	for (; len >= 5; len -= 5)
	{
		c0 = s[0];
		c1 = s[1];
		c2 = s[2];
		c3 = s[3];
		c4 = s[4];
		d[0] = alphabet[c0 >> 3];
		d[1] = alphabet[(c0 << 2 | c1 >> 6) & 0x1F];
		d[2] = alphabet[(c1 >> 1) & 0x1F];
		d[3] = alphabet[(c1 << 4 | c2 >> 4) & 0x1F];
		d[4] = alphabet[(c2 << 1 | c3 >> 7) & 0x1F];
		d[5] = alphabet[(c3 >> 2) & 0x1F];
		d[6] = alphabet[(c3 << 3 | c4 >> 5) & 0x1F];
		d[7] = alphabet[c4 & 0x1F];
		s += 5;
		d += 8;
	}

	c1 = 0;
	c2 = 0;
	c3 = 0;
	switch (len)
	{
	case 4:
		c3 = s[3];
		d[6] = alphabet[(c3 << 3) & 0x1F];	/* C4 == 0 */
		d[5] = alphabet[(c3 >> 2) & 0x1F];
	case 3:
		c2 = s[2];
		d[4] = alphabet[(c2 << 1 | c3 >> 7) & 0x1F];
	case 2:
		c1 = s[1];
		d[3] = alphabet[(c1 << 4 | c2 >> 4) & 0x1F];
		d[2] = alphabet[(c1 >> 1) & 0x1F];
	case 1:
		c0 = s[0];
		d[1] = alphabet[(c0 << 2 | c1 >> 6) & 0x1F];
		d[0] = alphabet[c0 >> 3];
	}

	d += XBASE32_ENCODED_LEN(len);
	return d - out;
}

ssize_t xbase32_encode(char *out, const void *in, size_t len)
{
	ssize_t rc = _encode(xbase32_alphabet, out, in, len);
	out[rc] = 0;
	return rc;
}

ssize_t xbase32_encode_nz(char *out, const void *in, size_t len)
{
	return _encode(xbase32_alphabet, out, in, len);
}

ssize_t xbase32_encode_upper(char *out, const void *in, size_t len)
{
	ssize_t rc = _encode(xbase32_Alphabet, out, in, len);
	out[rc] = 0;
	return rc;
}

ssize_t xbase32_encode_upper_nz(char *out, const void *in, size_t len)
{
	return _encode(xbase32_Alphabet, out, in, len);
}

ssize_t xbase32_decode(void *out, const char *in, size_t len)
{
	const unsigned char *s = (const unsigned char *)in, *end = s + len;
	char *d = (char *)out;
	bool find_end = false;
	int c = 0, x, r, r2, n;

	if ((ssize_t)len < 0)
	{
		end = (const unsigned char *)-1;
		find_end = true;
	}

	for (r2 = 0, r = 0, n = 0; s < end && (c = *s++) != 0; )
	{
		x = (c < 128) ? detab[c] : -1;
		if (x < 0)
		{
			if (x == NI && find_end)
				break;

			return -(s - (const unsigned char *)in);
		}

		++n;
		switch (n)
		{
		case 1:
			r = x << 3;
			break;
		case 2:
			*d++ = r + (x >> 2);
			r = x << 6;
			break;
		case 3:
			r2 = r;
			r = x << 1;
			break;
		case 4:
			*d++ = r2 + r + (x >> 4);
			r = x << 4;
			break;
		case 5:
			*d++ = r + (x >> 1);
			r = x << 7;
			break;
		case 6:
			r2 = r;
			r = x << 2;
			break;
		case 7:
			*d++ = r2 + r + (x >> 3);
			r = x << 5;
			break;
		case 8:
			*d++ = r + x;
			n = 0;
		}
	}

	if ((s < end && c != 0) 
		|| (n == 1 || n == 3 || n == 6)
		|| (n && (r & 0xff)))
	{
		return -(s - (const unsigned char *)in);
	}

	return d - (char *)out;
}

static char _luhn_char(const char *alphabet, const char *base32str, size_t len)
{
	const unsigned char *s = (const unsigned char *)base32str, *end = s + len;
	bool find_end = false;
	int c, x, n;
	int flip = 0;
	unsigned long sum[2] = {0, 0};

	if ((ssize_t)len < 0)
	{
		end = (const unsigned char *)-1;
		find_end = true;
	}

	for (; s < end && (c = *s++) != 0; )
	{
		x = (c < 128) ? detab[c] : -1;
		if (x < 0)
		{
			if (x == NI && find_end)
				break;

			return 0;
		}

		sum[flip] += (x * 2 / 32) + (x * 2 % 32);
		flip = !flip;
		sum[flip] += x;
	}

	flip = !flip;
	n = (sum[flip] % 32);
	return alphabet[n ? 32 - n : 0];
}

char xbase32_luhn_char(const char *b32str, size_t len)
{
	return _luhn_char(xbase32_alphabet, b32str, len);
}

char xbase32_luhn_Char(const char *b32str, size_t len)
{
	return _luhn_char(xbase32_Alphabet, b32str, len);
}

bool xbase32_luhn_check(const char *base32_with_luhn, size_t len)
{
	const unsigned char *s = (const unsigned char *)base32_with_luhn, *end = s + len;
	bool find_end = false;
	int c, x;
	int flip = 0;
	unsigned long sum[2] = {0, 0};

	if ((ssize_t)len < 0)
	{
		end = (const unsigned char *)-1;
		find_end = true;
	}

	if (len == 1)
		return false;

	while (s < end && (c = *s++) != 0)
	{
		x = (c < 128) ? detab[c] : -1;
		if (x < 0)
		{
			if (x == NI && find_end)
				break;

			return false;
		}

		sum[flip] += (x * 2 / 32) + (x * 2 % 32);
		flip = !flip;
		sum[flip] += x;
	}

	return (sum[flip] % 32) == 0;
}



#ifdef TEST_XBASE32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	const char *orig = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		;
	char buf[256], raw[256];
	int orig_len = strlen(orig);
	int len, i;

	len = xbase32_encode(buf, orig, orig_len);
	printf("base32: %d %s\n", len, buf);
	len = xbase32_encode_upper(buf, orig, orig_len);
	printf("BASE32: %d %s\n", len, buf);

	printf("luhn_char: %c\n", xbase32_luhn_char(buf, len));
	
	buf[len++] = xbase32_luhn_char(buf, len);
	buf[len] = 0;

	printf("luhn_check: %s\n", xbase32_luhn_check(buf, len) ? "ok" : "error");

	for (i = 0; i < 5; ++i, --orig_len)
	{
		xbase32_encode_upper(buf, orig, orig_len);
		len = xbase32_decode(raw, buf, -1);
		if (len != orig_len || memcmp(orig, raw, len) != 0)
		{
			printf("error\n");
			exit(1);
		}
	}

	return 0;
}

#endif

