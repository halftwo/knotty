#include "hamming7.h"

#define ERROR_IN_PARITY	0xFE

static const uint8_t _nibble2code_table[16] =
{ 
	0x00, 0x31, 0x52, 0x63, 0x64, 0x55, 0x36, 0x07,
	0x78, 0x49, 0x2A, 0x1B, 0x1C, 0x2D, 0x4E, 0x7F,
};

static const uint8_t _code2nibble_table[128] =
{ 
	0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x07, 0x07, 
	0x00, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x07, 
	0x00, 0x01, 0x02, 0x0B, 0x0C, 0x05, 0x06, 0x07, 
	0x0C, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C, 0x0C, 0x0B, 
	0x00, 0x01, 0x0A, 0x03, 0x04, 0x0D, 0x06, 0x07, 
	0x0A, 0x0D, 0x0A, 0x0A, 0x0D, 0x0D, 0x0A, 0x0D, 
	0x01, 0x01, 0x06, 0x01, 0x06, 0x01, 0x06, 0x06, 
	0x08, 0x01, 0x0A, 0x0B, 0x0C, 0x0D, 0x06, 0x0F, 
	0x00, 0x09, 0x02, 0x03, 0x04, 0x05, 0x0E, 0x07, 
	0x09, 0x09, 0x0E, 0x09, 0x0E, 0x09, 0x0E, 0x0E, 
	0x02, 0x05, 0x02, 0x02, 0x05, 0x05, 0x02, 0x05, 
	0x08, 0x09, 0x02, 0x0B, 0x0C, 0x05, 0x0E, 0x0F, 
	0x04, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x03, 
	0x08, 0x09, 0x0A, 0x03, 0x04, 0x0D, 0x0E, 0x0F, 
	0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0F, 
	0x08, 0x08, 0x08, 0x0F, 0x08, 0x0F, 0x0F, 0x0F,
};

#if 0
static const uint8_t _syndrome_table[8] =
{
	0x00,			/* 0  */
	ERROR_IN_PARITY,	/* 1  */
	ERROR_IN_PARITY,	/* 2  */
	0x01,			/* 3  */
	ERROR_IN_PARITY,	/* 4  */
	0x02,			/* 5  */
	0x04,			/* 6  */
	0x08,			/* 7  */
};

static int correct(uint8_t codeword, uint8_t *nibble)
{
	int rc = 0;
	uint8_t parity = (codeword >> 4) & 0x07;
	int syndrome = (_TO_CODE(codeword & 0x0F) >> 4) ^ parity;
	if (syndrome)
	{
		uint8_t correction = _syndrome_table[syndrome];
		if (correction != ERROR_IN_PARITY)
			codeword ^= correction;
		rc = 1;
	}

	*nibble = codeword & 0x0F;
	return rc;
}
#endif

static inline uint8_t _TO_CODE(uint8_t nibble)
{
	return _nibble2code_table[nibble];
}

static inline uint8_t _TO_NIBBLE(uint8_t code)
{
	return _code2nibble_table[code & 0x7F];
}


uint8_t hamming7_data2code(uint8_t nibble)
{
	return _TO_CODE(nibble & 0x0F);
}

int hamming7_code2data(uint8_t codeword, uint8_t *nibble)
{
	uint8_t nb, code;

	codeword &= 0x7F;
	nb = _code2nibble_table[codeword];
	code = _TO_CODE(nb);
	*nibble = nb;
	return code != codeword;
}

ssize_t hamming7_encode(uint8_t *out, const void *in, size_t len)
{
	const uint8_t *s = (const uint8_t *)in;
	uint8_t *d = out;
	uint8_t c0, c1, c2, c3, c4, c5, c6, c7;

	for (; len >= 4; len -= 4, s += 4)
	{
		c0 = _TO_CODE(s[0] >> 4);
		c1 = _TO_CODE(s[0] & 0x0F);
		c2 = _TO_CODE(s[1] >> 4);
		c3 = _TO_CODE(s[1] & 0x0F);
		c4 = _TO_CODE(s[2] >> 4);
		c5 = _TO_CODE(s[2] & 0x0F);
		c6 = _TO_CODE(s[3] >> 4);
		c7 = _TO_CODE(s[3] & 0x0F);

		*d++ = (c0 << 1) | (c1 >> 6);
		*d++ = (c1 << 2) | (c2 >> 5);
		*d++ = (c2 << 3) | (c3 >> 4);
		*d++ = (c3 << 4) | (c4 >> 3);
		*d++ = (c4 << 5) | (c5 >> 2);
		*d++ = (c5 << 6) | (c6 >> 1);
		*d++ = (c6 << 7) | c7;
	}

	c2 = 0;
	c3 = 0;
	c4 = 0;
	c5 = 0;
	switch (len)
	{
	case 3:
		c4 = _TO_CODE(s[2] >> 4);
		c5 = _TO_CODE(s[2] & 0x0F);
		d[4] = (c4 << 5) | (c5 >> 2);
		d[5] = (c5 << 6);
	case 2:
		c2 = _TO_CODE(s[1] >> 4);
		c3 = _TO_CODE(s[1] & 0x0F);
		d[2] = (c2 << 3) | (c3 >> 4);
		d[3] = (c3 << 4) | (c4 >> 3);
	case 1:
		c0 = _TO_CODE(s[0] >> 4);
		c1 = _TO_CODE(s[0] & 0x0F);
		d[0] = (c0 << 1) | (c1 >> 6);
		d[1] = (c1 << 2) | (c2 >> 5);
	}

	d += HAMMING7_LEN(len);
	return d - out;
}

ssize_t hamming7_decode(uint8_t *out, const void *in, size_t len)
{
	const uint8_t *s = (const uint8_t *)in;
	uint8_t *d = out;
	uint8_t c2 = 0;
	uint8_t hi = 0, low;
	int n = 0;

	while (len-- > 0)
	{
		uint8_t c = *s++;

		++n;
		switch (n)
		{
		case 1:
			hi = _TO_NIBBLE(c >> 1);
			c2 = (c << 6);
			break;
		case 2:
			low = _TO_NIBBLE(c2 | (c >> 2));
			c2 = (c << 5);
			*d++ = (hi << 4) | low;
			break;
		case 3:
			hi = _TO_NIBBLE(c2 | (c >> 3));
			c2 = (c << 4);
			break;
		case 4:
			low = _TO_NIBBLE(c2 | (c >> 4));
			c2 = (c << 3);
			*d++ = (hi << 4) | low;
			break;
		case 5:
			hi = _TO_NIBBLE(c2 | (c >> 5));
			c2 = (c << 2);
			break;
		case 6:
			low = _TO_NIBBLE(c2 | (c >> 6));
			c2 = (c << 1);
			*d++ = (hi << 4) | low;
			break;
		case 7:
			hi = _TO_NIBBLE(c2 | (c >> 7));
			low = _TO_NIBBLE(c);
			n = 0;
			*d++ = (hi << 4) | low;
			break;
		}
	}

	if (n == 1 || n == 3 || n == 5)
		goto error;

	if (n && (c2 & 0x7F))
		goto error;

	return d - out;
error:
	return -(s - (const uint8_t*)in);
}


#ifdef TEST_HAMMING7

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DATA_SIZE	(1024*100)
#define BUFF_SIZE	HAMMING7_LEN(DATA_SIZE)

int main()
{
	uint8_t buf1[BUFF_SIZE], buf2[BUFF_SIZE], buf3[BUFF_SIZE];
	ssize_t i, n, k;

	srandom(time(NULL));
	for (i = 0; i < DATA_SIZE; ++i)
	{
		buf1[i] = random();
	}

	n = hamming7_encode(buf2, buf1, DATA_SIZE);

	for (i = 0; i < n; i += 2)
	{
		int t = (random() % 7);
		buf2[i] ^= (1 << t);
	}
	
	k = hamming7_decode(buf3, buf2, n);

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

