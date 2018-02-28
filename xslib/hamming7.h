#ifndef HAMMING7_H_
#define HAMMING7_H_

#include "xsdef.h"
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define HAMMING7_LEN(n)	(((n) * 7 + 3) / 4)

/*
 * Only lower 4 bits of nibble are effective.
 * Return 7-bits codeword.
 */
uint8_t hamming7_data2code(uint8_t nibble);


/*
 * 0 means no errors (or more than 1 bit errors).
 * 1 means 1 bit error and corrected.
 */
int hamming7_code2data(uint8_t codeword, uint8_t *nibble);



/* Return the number of bytes placed in out. 
 */
ssize_t hamming7_encode(uint8_t *out, const void *in, size_t len);


/* Return the number of bytes placed in out. 
 * On error, return a negative number, the absolute value of the number
 * equals to the consumed size of the input string.
 */
ssize_t hamming7_decode(uint8_t *out, const void *in, size_t len);


#ifdef __cplusplus
}
#endif

#endif

