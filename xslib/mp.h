/* $Id: mp.h,v 1.11 2012/09/20 03:21:47 jiagui Exp $ */
#ifndef MP_H_
#define MP_H_

#include "ostk.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define HAVE_UINT128

#ifdef __cplusplus
extern "C" {
#endif


#ifdef HAVE_UINT128
	#define MP_BITS_PER_DIGIT	64
	#define MP_DIGIT_MAX		UINT64_MAX
	typedef uint64_t MP_DIGIT;
#else
	#define MP_BITS_PER_DIGIT	32
	#define MP_DIGIT_MAX		UINT32_MAX
	typedef uint32_t MP_DIGIT;
#endif


#define MP_NDIGITS(BITS)	(((BITS) + MP_BITS_PER_DIGIT - 1) / MP_BITS_PER_DIGIT)

#define MP_BUFSIZE(BITS)	(((BITS) + 7) / 8)


typedef MP_DIGIT* mpi_t;


/* The buf has size including the terminating zero.
   Return number of chars required excluding leading '0' and terminating zeroe.
   No leading '0' padded.
*/
size_t mp_to_hex(const mpi_t w, size_t ndigits, char *buf, size_t size);


/* The buf has size including the terminating zero.
   Return number of chars required excluding leading '0' and terminating zeroes.
   The buf is padded with leading '0' if neccessary.
 */
size_t mp_to_padhex(const mpi_t x, size_t ndigits, char *buf, size_t size);


/* Convert mpi_t into a byte string, in big-endian order.
   The leading zeros are eliminated.
   Return number of non-zero bytes required.
 */
size_t mp_to_buf(const mpi_t x, size_t ndigits, unsigned char *buf, size_t size);


/* Return number of bytes required excluding leading zeroes.
   The buf is padded with leading zeros if neccessary.
 */
size_t mp_to_padbuf(const mpi_t x, size_t ndigits, unsigned char *buf, size_t size);


size_t mp_from_hex(mpi_t w, size_t ndigits, const char *buf, size_t size);

size_t mp_from_buf(mpi_t w, size_t ndigits, const unsigned char *buf, size_t size);

void mp_from_digit(mpi_t w, size_t ndigits, MP_DIGIT x);

void mp_from_uint(mpi_t w, size_t ndigits, uintmax_t x);


size_t mp_digit_length(const mpi_t x, size_t ndigits);

size_t mp_bit_length(const mpi_t x, size_t ndigits);


void mp_zero(mpi_t w, size_t ndigits);

void mp_copy(mpi_t w, const mpi_t x, size_t xdigits);

void mp_assign(mpi_t w, size_t ndigits, const mpi_t x, size_t xdigits);


bool mp_iszero(const mpi_t x, size_t ndigits);

bool mp_equal(const mpi_t x, const mpi_t y, size_t ndigits);

int mp_compare(const mpi_t x, const mpi_t y, size_t ndigits);



/* w = x + y, return carry (1 or 0)
   NB: x may be added in place, i.e. w may be the same pointer as x.
 */
int mp_add_digit(mpi_t w, const mpi_t x, MP_DIGIT y, size_t ndigits);


/* w = x - y, return borrow (1 or 0)
   NB: x may be subtracted in place, i.e. w may be the same pointer as x.
 */
int mp_sub_digit(mpi_t w, const mpi_t x, MP_DIGIT y, size_t ndigits);


/* w = x * y, returns overflow
 */
MP_DIGIT mp_mul_digit(mpi_t w, const mpi_t x, MP_DIGIT y, size_t ndigits);


/* w = x / y, return x % y
 */
MP_DIGIT mp_div_digit(mpi_t w, const mpi_t x, MP_DIGIT y, size_t ndigits);



/* w = x + y, return carry (1 or 0)
   NB: x may be added in place, i.e. w may be the same pointer as x.
 */
int mp_add(mpi_t w, const mpi_t x, const mpi_t y, size_t ndigits);


/* w = x - y, return borrow (1 or 0)
   NB: x may be subtracted in place, i.e. w may be the same pointer as x.
 */
int mp_sub(mpi_t w, const mpi_t x, const mpi_t y, size_t ndigits);


/* w = x * y, w is 2 * ndigits long
 */ 
void mp_mul(mpi_t w, const mpi_t x, const mpi_t y, size_t ndigits);


/* w = x * x, w is 2 * ndigits long
 */ 
void mp_square(mpi_t w, const mpi_t x, size_t ndigits);



/**
   To reduce the memory allocation overhead, the chunk size of ostk_t
   should be at least 
	10 * sizeof(MP_DIGIT) * ndigits
 **/


/* r = x % m, r is mdigits long. 
   NB: may be the same pointer as x, or m.
 */
void mp_mod(mpi_t r, const mpi_t x, size_t xdigits, const mpi_t m, size_t mdigits, ostk_t *ostk);


/* w = (x + y) % m
   NB: w may be the same pointer as x, y, or m.
 */
void mp_modadd(mpi_t w, const mpi_t x, const mpi_t y, const mpi_t m, size_t ndigits, ostk_t *ostk);


/* w = (x - y) % m
   NB: w may be the same pointer as x, y, or m.
 */
void mp_modsub(mpi_t w, const mpi_t x, const mpi_t y, const mpi_t m, size_t ndigits, ostk_t *ostk);


/* w = (x * y) % m
   NB: w may be the same pointer as x, y, or m.
 */
void mp_modmul(mpi_t w, const mpi_t x, const mpi_t y, const mpi_t m, size_t ndigits, ostk_t *ostk);


/* w = (x ^ e) % m
   NB: w may be the same pointer as x, e, or m.

   The chunk size of ostk should be at least
	24 * sizeof(MP_DIGIT) * ndigits 	(when 240 < BITS(e) <= 768)
   and at most
	264 * sizeof(MP_DIGIT) * ndigits	(when BITS(e) > 4096)
 */
void mp_modexp(mpi_t w, const mpi_t x, const mpi_t e, const mpi_t m, size_t ndigits, ostk_t *ostk);


/* Same as mp_modexp(), except less memory needs
   NB: w may be the same pointer as x, e, or m.
 */
void mp_modexp_lessmem(mpi_t w, const mpi_t x, const mpi_t e, const mpi_t m, size_t ndigits, ostk_t *ostk);


#ifdef __cplusplus
}
#endif

#endif
