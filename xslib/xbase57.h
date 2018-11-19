#ifndef XBASE57_H_
#define XBASE57_H_ 1

#include "xsdef.h"
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * In xbase57, every 8-bytes block is encoded to 11 alphanumeric characters.
 */

#define XBASE57_ENCODED_LEN(n)	(((n) * 11 + 7) / 8)
#define XBASE57_DECODED_LEN(n)	(((n) * 8) / 11)

/* The alphabet is:
 *     ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789
 * Not used: IOl01
 */
extern const char xbase57_alphabet[];


/* Return the number of characters placed in out. 
 */
ssize_t xbase57_encode(char *out, const void *in, size_t len);

 
/* Return the number of characters placed in out. 
 * On error, return a negative number, the absolute value of the number
 * equals to the consumed size of the input string.
 * If ~len~ is negative, length of ~in~ is taken as strlen(~in~);
 */
ssize_t xbase57_decode(void *out, const char *in, size_t len);



/* Return the length of xbase57 string without the leading 'A'.
 * The _out_ string is nil-terminated.
 */
ssize_t xbase57_from_uint64(char *out, uint64_t n);


/* Return the len of xbase57 string not counting the leading 'A'.
 * Though _out_ is left-padded with 'A'.
 * The returned number may be larger than _len_.
 * NB: The _out_ string is NOT nil-terminated.
 */
ssize_t xbase57_pad_from_uint64(char *out, size_t len, uint64_t n);



/* Return the number of bits that count from the left first non-zero bit to the most right bit of the big number.
 * The returned number may be larger than 64.
 * If the second argument _len_ is -1, then the xbase57 string _b57str_ is nil-terminated.
 * -1 is returned if _b57str_ is not a valid xbase57 string.
 */
ssize_t xbase57_to_uint64(const char *b57str, size_t len, uint64_t* n);



#ifdef __cplusplus
}
#endif


#endif

