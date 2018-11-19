#include "xbase57.h"
#include "bit.h"
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#define NI	-3
#define SP	-2

static int8_t detab[128] = {
	NI, -1, -1, -1, -1, -1, -1, -1, -1, SP, SP, SP, SP, SP, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	SP, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, 49, 50, 51, 52, 53, 54, 55, 56, -1, -1, -1, -1, -1, -1,
	-1,  0,  1,  2,  3,  4,  5,  6,  7, -1,  8,  9, 10, 11, 12, -1,
	13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, -1, -1, -1, -1, -1,
	-1, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, -1, 35, 36, 37,
	38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, -1, -1, -1, -1, -1,
};

const char xbase57_alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";

ssize_t xbase57_encode(char *out, const void *in, size_t len)
{
	const unsigned char *data = (const unsigned char *)in;
	char *buf = out;

	for (; len >= 8; len -= 8)
	{
		int i;
		uint64_t acc = ((uint64_t)data[0] << 56)
			| ((uint64_t)data[1] << 48)
			| ((uint64_t)data[2] << 40)
			| ((uint64_t)data[3] << 32)
			| ((uint64_t)data[4] << 24)
			| ((uint64_t)data[5] << 16)
			| ((uint64_t)data[6] << 8)
			| data[7];
		data += 8;

		for (i = 10; i > 0; i--)
		{
			buf[i] = xbase57_alphabet[acc % 57];
			acc /= 57;
		}
		buf[0] = xbase57_alphabet[acc];
		buf += 11;
	}

	if (len > 0)
	{
		int i;
		int n = XBASE57_ENCODED_LEN(len);
		uint64_t acc = 0;
		for (i = 0; i < len; i++)
		{
			int shift = (7 - i) * 8;
			acc |= ((uint64_t)data[i] << shift);
		}

		for (i = 11 - n; i > 0; i--)
		{
			acc /= 57;
		}

		for (i = n - 1; i > 0; i--)
		{
			buf[i] = xbase57_alphabet[acc % 57];
			acc /= 57;
		}
		buf[0] = xbase57_alphabet[acc];
		buf += n;
	}

	*buf = 0;
	return buf - out;
}

ssize_t _do_decode(void *out, const char *in, size_t len, bool ignore_space)
{
	const char *src = in, *end = (const char *)-1;
	char *dst = (char *)out;
	bool find_end = true;
	uint64_t acc = 0;
	int cnt = 0;
	const char *last = src;

	if ((ssize_t)len >= 0)
	{
		end = src + len;
		find_end = false;
	}

	for (; src < end; ++src)
	{
		int ch = (unsigned char)*src;
		int de = (ch < 128) ? detab[ch] : -1;
		if (de < 0)
		{
			if (de == SP)
			{
				if (ignore_space)
					continue;
				return -(src + 1 - in);
			}

			if (de == NI && find_end)
				break;

			return -(src + 1 - in);
		}

		if (cnt < 10)
		{
			last = src;
			acc = acc * 57 + de;
			++cnt;
		}
		else
		{
			/*
			 * Detect overflow.  The largest 
			 * 11-letter possible is   "37UydrUNWM7" to encode 
			 * 0xffffffffffffffff, and "37UydrUNWM7" / 57 gives
			 * 0x047dc11f7047dc11 at this point (i.e.
			 * 0xffffffffffffffff / 57 = 0x047dc11f7047dc11)
			 */
			if (acc > UINT64_C(0x047dc11f7047dc11))
				return -(last + 1 - in);

			acc *= 57;
			if (acc > UINT64_C(0xffffffffffffffff) - de)
				return -(src + 1 - in);
			acc += de;

			*dst++ = acc >> 56;
			*dst++ = acc >> 48;
			*dst++ = acc >> 40;
			*dst++ = acc >> 32;
			*dst++ = acc >> 24;
			*dst++ = acc >> 16;
			*dst++ = acc >> 8;
			*dst++ = acc;

			acc = 0;
			cnt = 0;
		}
	}

	if (cnt)
	{
		static int8_t dlens[] = { 0, -1, 1, 2, -1, 3, 4, 5, -1, 6, 7, };
		static uint64_t maxes[] = {0, 
				UINT64_C(0x047944da05d3ad0c),	/* "3t999999999" / 57 */
				UINT64_C(0x047dbddd16ed96b0),	/* "37S99999999" / 57 */
				UINT64_C(0x047dc11aff23a734),	/* "37Ux5999999" / 57 */
				UINT64_C(0x047dc11f6c4df39d),	/* "37Uydj99999" / 57 */
				UINT64_C(0x047dc11f70446bdc),	/* "37UydrS9999" / 57 */
				UINT64_C(0x047dc11f7047d7ca),	/* "37UydrUNA99" / 57 */
				UINT64_C(0x047dc11f7047dc0d),	/* "37UydrUNWH9" / 57 */
		};

		int i;
		int shift;
		int dlen = dlens[cnt];
		if (dlen < 0)
			return -(last + 1 - in);

		for (i = cnt; i < 10; ++i)
			acc = acc * 57 + 56;

		if (acc > maxes[dlen])
			return -(last + 1 - in);

		acc = acc * 57 + 56;
		for (i = 0; i < dlen; i++)
		{
			shift = (7 - i) * 8;
			*dst++ = acc >> shift;
		}

		shift = (8 - dlen) * 8;
		acc >>= shift;
		acc <<= shift;

		for (i = 11 - cnt; i > 0; i--)
			acc /= 57;

		if (*last != xbase57_alphabet[acc%57])
			return -(last + 1 - in);
	}

	return dst - (char *)out;
}

ssize_t xbase57_decode(void *out, const char *in, size_t len)
{
	return _do_decode(out, in, len, true);
}


ssize_t xbase57_from_uint64(char *out, uint64_t n)
{
	int i, j;
	int k = 0;
	while (n > 0)
	{
		out[k] = xbase57_alphabet[n % 57];
		n /= 57;
		k++;
	}
	out[k] = 0;

	for (i = 0, j = k - 1; i < j; i++, j--)
	{
		char ch = out[i];
		out[i] = out[j];
		out[j] = ch;
	}

	return k;
}

ssize_t xbase57_pad_from_uint64(char *out, size_t len, uint64_t n)
{
	ssize_t k = 0;
	ssize_t i = len - 1;
	for (; i >= 0; i--)
	{
		if (n > 0)
		{
			out[i] = xbase57_alphabet[n % 57];
			n /= 57;
			k++;
		}
		else
		{
			out[i] = xbase57_alphabet[0];
		}
	}

	while (n > 0)
	{
		n /= 57;
		k++;
	}

	return k;
}

static int _block_to_uint64(const char *block, int len, uint64_t *n)
{
	int i, k;
	uint64_t acc = 0;

	if (len > 11)
		len = 11;

	k = len < 10 ? len : 10;
	for (i = 0; i < k; i++)
	{
		int ch = (unsigned char)block[i];
		int de = (ch < 128) ? detab[ch] : -1;
		if (de < 0)
			return i;
		acc = acc * 57 + de;
	}

	if (len > 10)
	{
		if (acc > UINT64_C(0x047dc11f7047dc11))
			return false;
		acc *= 57;

		int ch = (unsigned char)block[i];
		int de = (ch < 128) ? detab[ch] : -1;
		if (de < 0)
			return i;
		if (acc > UINT64_C(0xffffffffffffffff) - de)
			return i;
		acc += de;
	}

	*n = acc;
	return len;
}

ssize_t xbase57_to_uint64(const char *b57str, size_t len, uint64_t* n)
{
	int nz_k = 0;
	uint64_t nz_value = 0;
	int k;

	if ((ssize_t)len < 0)
		len = strlen(b57str);

	for (k = 0; len > 0; k++)
	{
		uint64_t value = 0;
		int num = len >= 11 ? 11 : len;
		len -= num;
		if (_block_to_uint64(b57str + len, num, &value) < num)
			return -1;

		if (k == 0)
			*n = value;

		if (value != 0)
		{
			nz_k = k;
			nz_value = value;
		}
	}

	if (nz_value == 0)
		return 0;

	return nz_k * 64 + bit_msb64_find(nz_value) + 1;
}


#ifdef TEST_XBASE57

#include "opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void usage(const char *prog)
{
	fprintf(stderr, "usage: %s -e string\n", prog);
	fprintf(stderr, "       %s -x -d string\n", prog);
	fprintf(stderr, "       %s -t\n", prog);
	fprintf(stderr, "       %s -f file\n", prog);
	exit(1);
}

int main(int argc, char **argv)
{
	char buf[1024];
	char *prog = argv[0];
	int optend;
	const char *str = NULL;
	const char *file = NULL;
	bool test = false;
	bool decode = false;
	bool encode = false;
	bool hex = false;

	OPT_BEGIN(argc, argv, &optend) {
	case 'e':
		encode = true;
		str = OPT_EARG(usage(prog));
		break;
	case 'd':
		decode = true;
		str = OPT_EARG(usage(prog));
		break;
	case 't':
		test = true;
		break;
	case 'f':
		file = OPT_EARG(usage(prog));
		break;
	case 'x':
		hex = true;
		break;
	default:
		usage(prog);
	} OPT_END();

	if (encode)
	{
		int len = strlen(str);
		xbase57_encode(buf, str, len);
		printf("encoded: %s\n", buf);
	}
	else if (decode)
	{
		int inlen = strlen(str);
		int len = xbase57_decode(buf, str, inlen);
		if (len > 0)
		{
			printf("decoded %d: ", len);
			if (hex)
			{
				int i;
				for (i = 0; i < len; i++)
					printf("%02x", (uint8_t)buf[i]);
			}
			else
			{
				printf("%.*s", len, buf);
			}
			printf("\n");
		}
		else
			fprintf(stderr, "decode failed\n");
	}
	else if (test)
	{
		int i, k;
		char t[8] = { -1, -1, -1, -1, -1, -1, -1, -1, };
		for (i = 1; i <= 8; i++)
		{
			xbase57_encode(buf, t, i);
			printf("encoded %d ", i);
			for (k = 0; k < i; k++)
				fputs("FF", stdout);
			for (k = i; k < 8; k++)
				fputs("  ", stdout);
			printf(" : %s\n", buf);
		}
	}
	else if (file != NULL)
	{
		FILE *fp = fopen(file, "rb");
		if (fp)
		{
			while (1)
			{
				char buf[48];
				char line[80];
				int n = fread(buf, 1, sizeof(buf), fp);
				if (n > 0)
				{
					int len = xbase57_encode(line, buf, n);
					line[len++] = '\n';
					fwrite(line, 1, len, stdout);
				}
				else
					break;
			}
			fclose(fp);
		}
	}
	else
	{
		usage(prog);
	}
	return 0;
}

#endif
