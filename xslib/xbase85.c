#include "xbase85.h"
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#define NI	-3
#define SP	-2

static int8_t detab[256] = {
	NI, -1, -1, -1, -1, -1, -1, -1, -1, SP, SP, SP, SP, SP, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	SP, 62, -1, 63, 64, 65, 66, -1, 67, 68, 69, 70, -1, 71, -1, -1,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, 72, 73, 74, 75, 76,
	77, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, 78, 79,
	80, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
	51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 81, 82, 83, 84, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

const char xbase85_alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~";

ssize_t xbase85_encode(char *out, const void *in, size_t len)
{
	const unsigned char *data = (const unsigned char *)in;
	char *buf = out;

	for (; len >= 4; len -= 4)
	{
		uint_fast32_t acc = ((uint_fast32_t)data[0] << 24)
			| ((uint_fast32_t)data[1] << 16)
			| ((uint_fast32_t)data[2] << 8)
			| data[3];
		data += 4;

		buf[4] = xbase85_alphabet[acc % 85];
		acc /= 85;
		buf[3] = xbase85_alphabet[acc % 85];
		acc /= 85;
		buf[2] = xbase85_alphabet[acc % 85];
		acc /= 85;
		buf[1] = xbase85_alphabet[acc % 85];
		acc /= 85;
		buf[0] = xbase85_alphabet[acc];
		buf += 5;
	}

	if (len > 0)
	{
		uint_fast32_t acc = ((uint_fast32_t)data[0] << 24);
		if (len >= 2)
		{
			acc |= ((uint_fast32_t)data[1] << 16);
			if (len >= 3)
				acc |= ((uint_fast32_t)data[2] << 8);
		}

		acc /= 85;
		if (len >= 3)
			buf[3] = xbase85_alphabet[acc % 85];

		acc /= 85;
		if (len >= 2)
			buf[2] = xbase85_alphabet[acc % 85];

		acc /= 85;
		buf[1] = xbase85_alphabet[acc % 85];
		acc /= 85;
		buf[0] = xbase85_alphabet[acc];
		buf += len + 1;
	}

	*buf = 0;
	return buf - out;
}

ssize_t xbase85_decode(void *out, const char *in, size_t len)
{
	const char *src = in, *end = (const char *)-1;
	char *dst = (char *)out;
	bool find_end = true;
	uint_fast32_t acc = 0;
	int cnt = 0;
	const char *last = src;

	if ((ssize_t)len >= 0)
	{
		end = src + len;
		find_end = false;
	}

	for (; src < end; ++src)
	{
		int de = detab[(unsigned char)*src];
		if (de < 0)
		{
			if (de == SP)
				continue;

			if (de == NI && find_end)
				break;

			return -(src + 1 - in);
		}

		if (cnt < 4)
		{
			last = src;
			acc = acc * 85 + de;
			++cnt;
		}
		else
		{
			/*
			 * Detect overflow.  The largest
			 * 5-letter possible is "|NsC0" to
			 * encode 0xffffffff, and "|NsC" gives
			 * 0x03030303 at this point (i.e.
			 * 0xffffffff = 0x03030303 * 85).
			 */
			if (0x03030303 < acc)
				return -(last + 1 - in);
			if (0xffffffff - de < (acc *= 85))
				return -(src + 1 - in);
			acc += de;

			*dst++ = acc >> 24;
			*dst++ = acc >> 16;
			*dst++ = acc >> 8;
			*dst++ = acc;

			acc = 0;
			cnt = 0;
		}
	}

	if (cnt == 1)
		return -(last + 1 - in);

	if (cnt)
	{
		int i;

		for (i = cnt; i < 4; ++i)
			acc *= 85;

		if (0x03030300 < acc || (0x03030000 < acc && cnt == 3) || (0x03000000 < acc && cnt == 2))
			return -(last + 1 - in);
		acc *= 85;
		acc += 0xffffff >> (cnt - 2) * 8;

		if (cnt == 2)
		{
			acc &= ~(uint_fast32_t)0xffffff;
			if (*last != xbase85_alphabet[(acc / (85 * 85 * 85)) % 85])
				return -(last + 1 - in);
		}
		else if (cnt == 3)
		{
			acc &= ~(uint_fast32_t)0xffff;
			if (*last != xbase85_alphabet[(acc / (85 * 85)) % 85])
				return -(last + 1 - in);
		}
		else
		{
			acc &= ~(uint_fast32_t)0xff;
			if (*last != xbase85_alphabet[(acc / 85) % 85])
				return -(last + 1 - in);
		}

		*dst++ = acc >> 24;
		if (cnt >= 3)
		{
			*dst++ = acc >> 16;
			if (cnt >= 4)
				*dst++ = acc >> 8;
		}
	}

	return dst - (char *)out;
}


#ifdef TEST_XBASE85

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	char buf[1024];
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s -e string\n", argv[0]);
		fprintf(stderr, "       %s -d string\n", argv[0]);
		fprintf(stderr, "       %s -t\n", argv[0]);
		fprintf(stderr, "       %s -f file\n", argv[0]);
		exit(1);
	}

	if (!strcmp(argv[1], "-e"))
	{
		int len = strlen(argv[2]);
		xbase85_encode(buf, argv[2], len);
		printf("encoded: %s\n", buf);
		return 0;
	}

	if (!strcmp(argv[1], "-d"))
	{
		int inlen = strlen(argv[2]);
		int len = xbase85_decode(buf, argv[2], inlen);
		if (len > 0)
			printf("decoded: %.*s\n", len, buf);
		else
			fprintf(stderr, "decode failed\n");
		return 0;
	}

	if (!strcmp(argv[1], "-t"))
	{
		char t[4] = { -1, -1, -1, -1 };
		xbase85_encode(buf, t, 4);
		printf("encoded: %s\n", buf);
		return 0;
	}

	if (!strcmp(argv[1], "-f"))
	{
		FILE *fp = fopen(argv[2], "rb");
		if (fp)
		{
			while (1)
			{
				char buf[52];
				char line[80];
				int n = fread(buf, 1, sizeof(buf), fp);
				if (n > 0)
				{
					int len = xbase85_encode(line, buf, n);
					line[len++] = '\n';
					fwrite(line, 1, len, stdout);
				}
				else
					break;
			}
			fclose(fp);
		}
	}
}

#endif
