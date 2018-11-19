/*
   The used character set is defined in http://www.crockford.com/wrmg/base32.html

   Author: XIONG Jiagui
   Date: 2009-12-29
 */
#ifndef XBASE32_H_
#define XBASE32_H_ 1

#include "xsdef.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define XBASE32_ENCODED_LEN(n)	(((n) * 8 + 4) / 5)
#define XBASE32_DECODED_LEN(n)	(((n) * 5) / 8)

/*
 * Not used alphabetic characters:
 *	ILOU
 */

/*  0123456789abcdefghjkmnpqrstvwxyz  */
extern const char xbase32_alphabet[];

/*  0123456789ABCDEFGHJKMNPQRSTVWXYZ  */
extern const char xbase32_Alphabet[];


/* Return the number of characters placed in out. 
 */
ssize_t xbase32_encode(char *out, const void *in, size_t len);
ssize_t xbase32_encode_nz(char *out, const void *in, size_t len);
ssize_t xbase32_encode_upper(char *out, const void *in, size_t len);
ssize_t xbase32_encode_upper_nz(char *out, const void *in, size_t len);

 
/* Return the number of characters placed in out. 
 * On error, return a negative number, the absolute value of the number
 * equals to the consumed size of the input string.
 * If ~len~ is negative, length of ~in~ is taken as strlen(~in~);
 */
ssize_t xbase32_decode(void *out, const char *in, size_t len);



char xbase32_luhn_char(const char *base32str, size_t len);
char xbase32_luhn_Char(const char *base32str, size_t len);


bool xbase32_luhn_check(const char *base32_with_luhn, size_t len);



/* Return the len of xbase32 string without the leading '0'.
 * NB: The _out_ string is nil-terminated.
 */
ssize_t xbase32_from_uint64(char *out, uint64_t n);


/* Return the len of xbase32 string not counting the leading '0'.
 * Though _out_ is left-padded with '0'.
 * The returned number may be larger than _len_.
 * NB: The _out_ string is NOT nil-terminated.
 */
ssize_t xbase32_pad_from_uint64(char *out, size_t len, uint64_t n);


/* Return the number of bits that count from the left first non-zero bit to the most right bit of the big number.
 * The returned number may be larger than 64.
 * If the second argument _len_ is -1, then the xbase32 string _b32str_ is nil-terminated.
 * -1 is returned if _b32str_ is not a valid xbase32 string.
 */
ssize_t xbase32_to_uint64(const char *b32str, size_t len, uint64_t* n);


#ifdef __cplusplus
}
#endif

#endif

