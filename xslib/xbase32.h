/* $Id: xbase32.h 384 2016-10-13 01:42:59Z gremlin $ */
/*
   The used character set is defined in http://www.crockford.com/wrmg/base32.html

   Author: XIONG Jiagui
   Date: 2009-12-29
 */
#ifndef XBASE32_H_
#define XBASE32_H_ 1

#include "xsdef.h"
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define XBASE32_LEN(n)	(((n) * 8 + 4) / 5)

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



#ifdef __cplusplus
}
#endif

#endif

