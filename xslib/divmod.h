/* $Id: divmod.h,v 1.5 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-08-02
 */
#ifndef DIVMOD_H_
#define DIVMOD_H_ 1

#include <stdint.h>


/* 
   see: Daan Leijen, "Division and Modulus for Computer Scientists"
   for more informations.
 */


/* Truncated division, T-division 
   The sign of the modulus is always the same as the sign of the dividend.
   T-division truncates the quotient and effectively rounds towards zero.
   T-division is used by virtually all modern processors and is adopted 
   by the ISO C99 standard.
 */
#define truncated_div(D, d)	((D) / (d))
#define truncated_mod(D, d)	((D) % (d))


/* Floored division, F-division
   The sign of the modulus is always the same as the sign of the divisor.
   F-division floors the quotient and effectively rounds toward negative 
   infinity.
   F-division is described by Knuth. 
   F-division is sign-preserving, ie. given the signs of the quotient and 
   the remainder, we can give the signs of the dividend and divisor. 
 */
#define floored_div(D, d)	(floored_div)((D), (d))
#define floored_mod(D, d)	(floored_mod)((D), (d))


/* Euclidean division, E-division
   The modulus is always non-negative.
   E-division is described by Boute.
   Boute argues the Euclidean division is superior to the other ones in terms 
   of regularity and useful mathematical properties.
 */
#define euclidean_div(D, d)	(euclidean_div)((D), (d))
#define euclidean_mod(D, d)	(euclidean_mod)((D), (d))


/*
   The following table compares results of the different division 
   definitions for some inputs.

   -------------------------------------------
     (D, d)  | (qT, rT) | (qF, rF) | (qE, rE)
   ----------+----------+----------+----------
    (+8, +3) | (+2, +2) | (+2, +2) | (+2, +2)
    (+8, -3) | (-2, +2) | (-3, -1) | (-2, +2)
    (-8, +3) | (-2, -2) | (-3, +1) | (-3, +1)
    (-8, -3) | (+2, -2) | (+2, -2) | (+3, +1)
   ----------+----------+----------+----------
    (+1, +2) | ( 0, +1) | ( 0, +1) | ( 0, +1)
    (+1, -2) | ( 0, +1) | (-1, -1) | ( 0, +1)
    (-1, +2) | ( 0, -1) | (-1, +1) | (-1, +1)
    (-1, -2) | ( 0, -1) | ( 0, -1) | (+1, +1)
   -------------------------------------------

 */

inline intmax_t (floored_div)(intmax_t D, intmax_t d)
{
	intmax_t q = truncated_div(D, d);
	intmax_t r = truncated_mod(D, d);
	if ((r < 0 && d > 0) || (d < 0 && r > 0))
		--q;
	return q;
}

inline intmax_t (floored_mod)(intmax_t D, intmax_t d)
{
	intmax_t r = truncated_mod(D, d);
	if ((r < 0 && d > 0) || (d < 0 && r > 0))
		r += d;
	return r;
}

inline intmax_t (euclidean_div)(intmax_t D, intmax_t d)
{
	intmax_t q = truncated_div(D, d);
	intmax_t r = truncated_mod(D, d);
	if (r < 0)
	{
		if (d > 0) --q;
		else ++q;
	}
	return q;
}

inline intmax_t (euclidean_mod)(intmax_t D, intmax_t d)
{
	intmax_t r = truncated_mod(D, d);
	if (r < 0)
	{
		if (d > 0) r += d;
		else r -= d;
	}
	return r;
}

#endif

