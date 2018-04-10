/* $Id: bit.h,v 1.8 2012/09/20 03:21:47 jiagui Exp $ */
/* 
   Author: XIONG Jiagui
   Date: 2005-06-20
 */
#ifndef BIT_H_
#define BIT_H_ 1

#include "xsdef.h"
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define BITMAP_SET(map, p)	((void)(((uint8_t*)(map))[(p) / 8] |= 1 << ((p) % 8)))
#define BITMAP_CLEAR(map, p)	((void)(((uint8_t*)(map))[(p) / 8] &= ~(1 << ((p) % 8))))
#define BITMAP_FLIP(map, p)	((void)(((uint8_t*)(map))[(p) / 8] ^= 1 << ((p) % 8)))
#define BITMAP_TEST(map, p)	(((char*)(map))[(p) / 8] & (1 << ((p) % 8)))


#define IS_POWER_TWO(w)		(((w) & -(w)) == (w))
#define BIT_IS_SUBSET(sub, u)	(((sub) & (u)) == (sub))
#define BIT_LEFT_ROTATE(w, s)	(((w) << (s)) | ((w) >> (sizeof(w)*8-(s))))
#define BIT_RIGHT_ROTATE(w, s)	(((w) << (sizeof(w)*8-(s))) | ((w) >> (s)))
#define BIT_ALIGN(n, align)	(((n) + (align) - 1) & ~((align) - 1))


ssize_t bitmap_lsb_find1(const uint8_t *bitmap, size_t start, size_t end);
ssize_t bitmap_lsb_find0(const uint8_t *bitmap, size_t start, size_t end);

bool bitmap_msb_equal(const uint8_t *bmap1, const uint8_t *bmap2, size_t prefix);
bool bitmap_lsb_equal(const uint8_t *bmap1, const uint8_t *bmap2, size_t prefix);

bool bit_msb32_equal(uint32_t a, uint32_t b, size_t prefix);
bool bit_lsb32_equal(uint32_t a, uint32_t b, size_t prefix);

bool bit_msb64_equal(uint64_t a, uint64_t b, size_t prefix);
bool bit_lsb64_equal(uint64_t a, uint64_t b, size_t prefix);

int bit_parity(uint32_t w);
int bit_count(uintmax_t x);


/* Find the last bit set or the first bit set.
 * The MSB is position 31 (or 63), the LSB is position 0. 
 */
int bit_msb32_find(uint32_t n);
int bit_lsb32_find(uint32_t n);

int bit_msb64_find(uint64_t n);
int bit_lsb64_find(uint64_t n);

uintmax_t round_up_power_two(uintmax_t x);
uintmax_t round_down_power_two(uintmax_t x);


#ifdef __cplusplus
}
#endif

#endif

