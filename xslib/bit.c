/*
   Author: XIONG Jiagui
   Date: 2005-06-20
 */
#include "bit.h"
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: bit.c,v 1.7 2012/09/20 03:21:47 jiagui Exp $";
#endif

ssize_t bitmap_lsb_find1(const uint8_t *bitmap, size_t start, size_t end)
{
	size_t i, s, e, b, bstart, bend;
	if (start >= end)
		return -1;

	s = start / 8;
	i = start % 8;
	if ((bitmap[s] >> i) != 0)
	{
		b = s;
		bstart = i;
		bend = 8;
	}
	else
	{
		e = (end - 1) / 8;
		for (b = s + 1; b <= e && bitmap[b] == 0; ++b)
			continue;
		if (b > e)
			return -1;

		if (b == e)
		{
			bend = end % 8;
			if (bend == 0)
				bend = 8;
		}
		else
		{
			bend = 8;
		}
		bstart = 0;
	}

	for (i = bstart; i < bend; ++i)
	{
		if (bitmap[b] & (1 << i))
			return b * 8 + i;
	}

	return -1;
}

ssize_t bitmap_lsb_find0(const uint8_t *bitmap, size_t start, size_t end)
{
	size_t i, s, e, b, bstart, bend;
	if (start >= end)
		return -1;

	s = start / 8;
	i = start % 8;
	if ((bitmap[s] >> i) != (0xFF >> i))
	{
		b = s;
		bstart = i;
		bend = 8;
	}
	else
	{
		e = (end - 1) / 8;
		for (b = s + 1; b <= e && bitmap[b] == 0xFF; ++b)
			continue;
		if (b > e)
			return -1;

		if (b == e)
		{
			bend = end % 8;
			if (bend == 0)
				bend = 8;
		}
		else
		{
			bend = 8;
		}
		bstart = 0;
	}

	for (i = bstart; i < bend; ++i)
	{
		if (!(bitmap[b] & (1 << i)))
			return b * 8 + i;
	}

	return -1;
}

int bit_parity(uint32_t w)
{
	w ^= (w >> 16);
	w ^= (w >> 8);
	w ^= (w >> 4);
	w ^= (w >> 2);
	w ^= (w >> 1);
	return (w & 1);
}

int bit_count(uintmax_t x)
{
	int n = 0;

	if (x)
	{
		do
		{
			n++;            
		} while ((x &= x-1));
	}

	return n;
}

uintmax_t round_up_power_two(uintmax_t x)
{
	int r = 1;

	if (x == 0 || (intmax_t)x < 0)
		return 0;
	--x;
	while (x >>= 1)
		r++;
	return (UINTMAX_C(1) << r);
}

uintmax_t round_down_power_two(uintmax_t x)
{
	int r = 0;

	if (x == 0)
		return 0;
	while (x >>= 1)
		r++;
	return (UINTMAX_C(1) << r);
}

bool bitmap_msb_equal(const uint8_t *bmap1, const uint8_t *bmap2, size_t prefix)
{
	int bytes = prefix / 8;
	int bits = prefix % 8;

	if (bytes)
	{
		if (memcmp(bmap1, bmap2, bytes))
			return false;
	}
	
	if (bits)
	{
		uint8_t b1 = bmap1[bytes] >> (8 - bits);
		uint8_t b2 = bmap2[bytes] >> (8 - bits);
		return b1 == b2;
	}

	return true;
}

bool bitmap_lsb_equal(const uint8_t *bmap1, const uint8_t *bmap2, size_t prefix)
{
	int bytes = prefix / 8;
	int bits = prefix % 8;

	if (bytes)
	{
		if (memcmp(bmap1, bmap2, bytes))
			return false;
	}
	
	if (bits)
	{
		uint8_t b1 = bmap1[bytes] << (8 - bits);
		uint8_t b2 = bmap2[bytes] << (8 - bits);
		return b1 == b2;
	}

	return true;
}

bool bit_msb32_equal(uint32_t a, uint32_t b, size_t prefix)
{
	if (prefix < 32)
	{
		a >>= (32 - prefix);
		b >>= (32 - prefix);
	}
	return a == b;
}

bool bit_lsb32_equal(uint32_t a, uint32_t b, size_t prefix)
{
	if (prefix < 32)
	{
		a <<= (32 - prefix);
		b <<= (32 - prefix);
	}
	return a == b;
}

bool bit_msb64_equal(uint64_t a, uint64_t b, size_t prefix)
{
	if (prefix < 64)
	{
		a >>= (64 - prefix);
		b >>= (64 - prefix);
	}
	return a == b;
}

bool bit_lsb64_equal(uint64_t a, uint64_t b, size_t prefix)
{
	if (prefix < 64)
	{
		a <<= (64 - prefix);
		b <<= (64 - prefix);
	}
	return a == b;
}

