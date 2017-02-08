#ifndef GOLAY24_H_
#define GOLAY24_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Only lower 12 bits of data are effective.
 * Return 24-bits codeword (including one parity bit).
 */
uint32_t golay24_data2code(uint32_t data);


/*
 * 0 means no errors.
 * -1 means corrections not possible.
 * 1~3 means corrected that many of errors.
 */
int golay24_code2data(uint32_t codeword, uint32_t *data);



#ifdef __cplusplus
}
#endif

#endif
