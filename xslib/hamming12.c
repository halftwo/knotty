#include "hamming12.h"

#define FATAL_ERROR	0xFF
#define ERROR_IN_PARITY	0xFE

static const uint8_t _low_nibble_parity[16] =
{ 0,  3,  5,  6,  6,  5,  3,  0,  7,  4,  2,  1,  1,  2,  4,  7 };

static const uint8_t _high_nibble_parity[16] = 
{ 0,  9, 10,  3, 11,  2,  1,  8, 12,  5,  6, 15,  7, 14, 13,  4 };


static const uint8_t _syndrome_table[16] =
{
	0x00,			/* 0  */
	ERROR_IN_PARITY,	/* 1  */
	ERROR_IN_PARITY,	/* 2  */
	0x01,			/* 3  */
	ERROR_IN_PARITY,	/* 4  */
	0x02,			/* 5  */
	0x04,			/* 6  */
	0x08,			/* 7  */
	ERROR_IN_PARITY,	/* 8  */
	0x10,			/* 9  */
	0x20,			/* 10 */
	0x40,			/* 11 */
	0x80,			/* 12 */
	FATAL_ERROR,		/* 13 */
	FATAL_ERROR,		/* 14 */
	FATAL_ERROR,		/* 15 */
};


uint8_t hamming12_parity(uint8_t value)
{
	return _low_nibble_parity[value & 0x0F] ^ _high_nibble_parity[value >> 4];
}


int hamming12_correct(uint8_t *value, uint8_t parity)
{
	int syndrome = hamming12_parity(*value) ^ (parity & 0x0F);
	if (syndrome)
	{
		uint8_t correction = _syndrome_table[syndrome];
		if (correction == FATAL_ERROR)
			return -1;

		if (correction != ERROR_IN_PARITY)
			*value ^= correction;

		return 1;
	}
	return 0;
}	

ssize_t hamming12_encode(uint8_t *out, const void *in, size_t len)
{
	const uint8_t *s = (const uint8_t *)in;
	uint8_t *d = out;

	for (; len >= 2; len -= 2)
	{
		uint8_t low = hamming12_parity(s[0]);
		uint8_t high = hamming12_parity(s[1]);
		*d++ = s[0];
		*d++ = (high << 4) | low;
		*d++ = s[1];
		s += 2;
	}

	if (len)
	{
		uint8_t low = hamming12_parity(s[0]);
		*d++ = s[0];
		*d++ = low;
	}

	return d - out;
}

ssize_t hamming12_decode(uint8_t *out, const void *in, size_t len)
{
	const uint8_t *s = (const uint8_t *)in;
	uint8_t *d = out;

	for (; len >= 3; len -= 3, s += 3)
	{
		uint8_t byte1  = s[0];
		uint8_t parity = s[1];
		uint8_t byte2  = s[2];
		if (hamming12_correct(&byte1, parity & 0x0F) < 0)
		{
			goto error;
		}

		if (hamming12_correct(&byte2, parity >> 4) < 0)
		{
			s += 2;
			goto error;
		}

		*d++ = byte1;
		*d++ = byte2;
	}

	if (len)
	{
		uint8_t byte = s[0];
		if (len == 1)
			goto error;

		if (hamming12_correct(&byte, s[1] & 0x0F) < 0)
			goto error;

		*d++ = byte;
	}

	return d - out;
error:
	return -(s - (const uint8_t*)in);
}


#ifdef TEST_HAMMING12

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DATA_SIZE	(1024*100)
#define BUFF_SIZE	HAMMING12_LEN(DATA_SIZE)

int main()
{
	uint8_t buf1[BUFF_SIZE], buf2[BUFF_SIZE], buf3[BUFF_SIZE];
	ssize_t i, n, k;

	srandom(time(NULL));
	for (i = 0; i < DATA_SIZE; ++i)
	{
		buf1[i] = random();
	}

	n = hamming12_encode(buf2, buf1, DATA_SIZE);

	for (i = 0; i < n; i += 3)
	{
		int t = (random() % 12);
		if (t < 8)
			buf2[i] ^= (1 << t);
		else
			buf2[i+1] ^= (1 << (t - 8));

		t = (random() % 12);
		if (t < 4)
			buf2[i+1] ^= (1 << (t + 4));
		else
			buf2[i+2] ^= (1 << (t - 4));
	}
	
	k = hamming12_decode(buf3, buf2, n);

	if (k != DATA_SIZE)
	{
		fprintf(stderr, "decode failed\n");
		exit(1);
	}

	if (memcmp(buf1, buf3, DATA_SIZE) != 0)
	{
		fprintf(stderr, "data corrupted\n");
		exit(1);
	}

	fprintf(stderr, "success\n");
	return 0;
}

#endif

