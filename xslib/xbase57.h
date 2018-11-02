#ifndef XBASE57_H_
#define XBASE57_H_ 1

#include "xsdef.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * In xbase57, every 8-bytes block is encoded to 11 letters.
 */

#define XBASE57_ENCODED_LEN(n)	(((n) * 11 + 7) / 8)
#define XBASE57_DECODED_LEN(n)	(((n) * 8) / 11)

/* The alphabet is:
 *     0123456789ABCDEFGHJKLMNPQRSTVWXYZabcdefghijkmnpqrstuvwxyz
 * Not used: IOUlo
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


#ifdef __cplusplus
}
#endif


#endif


