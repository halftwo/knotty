/* $Id$ */
#ifndef DECIMAL64_H_
#define DECIMAL64_H_

#include "decDouble.h"
#include "xstr.h"
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define DECIMAL64_STRING_MAX	DECDOUBLE_String	/* 24 + 1 */

#define DECIMAL64_ERR_OVERFLOW		-2
#define DECIMAL64_ERR_UNDERFLOW		-3
#define DECIMAL64_ERR_NAN		-4
#define DECIMAL64_ERR_ROUND		-5
#define DECIMAL64_ERR_LOST_DIGITS	-6


typedef decDouble decimal64_t;


extern const decimal64_t decimal64_zero;
extern const decimal64_t decimal64_minus_zero;


size_t decimal64_to_cstr(decimal64_t dec, char buf[]);


/* Follwoing functions return 0 if success, or return following
   code on error:
	DECIMAL64_ERR_OVERFLOW
	DECIMAL64_ERR_UNDERFLOW
	DECIMAL64_ERR_NAN
	DECIMAL64_ERR_ROUND
	DECIMAL64_ERR_LOST_DIGITS
   No matter error or not, the result value is always set 
   appropriately.
 */

int decimal64_to_integer(decimal64_t dec, intmax_t *pv);

int decimal64_from_integer(decimal64_t *dec, intmax_t v);


int decimal64_from_xstr(decimal64_t *dec, const xstr_t *xs, xstr_t *end/*NULL*/);

int decimal64_from_cstr(decimal64_t *dec, const char *str, char **end/*NULL*/);



#ifdef __cplusplus
}
#endif

#endif
