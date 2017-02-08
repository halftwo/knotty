#ifndef HAMMING12_H_
#define HAMMING12_H_

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define HAMMING12_LEN(n)	(((n) * 12 + 7) / 8)

/*
 * Return 4 bits parity.
 */
uint8_t hamming12_parity(uint8_t byte);


/*
 * 0 means no errors.
 * 1 means 1 bit error and corrected.
 * -1 means corrections not possible.
 */
int hamming12_correct(uint8_t *byte, uint8_t parity);



/* Return the number of bytes placed in out. 
 */
ssize_t hamming12_encode(uint8_t *out, const void *in, size_t len);


/* Return the number of bytes placed in out. 
 * On error, return a negative number, the absolute value of the number
 * equals to the consumed size of the input string.
 */
ssize_t hamming12_decode(uint8_t *out, const void *in, size_t len);


#ifdef __cplusplus
}
#endif

#endif
