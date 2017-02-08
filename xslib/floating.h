/* $Id: floating.h,v 1.3 2014/11/27 06:30:15 gremlin Exp $ */
#ifndef FLOATING_H_
#define FLOATING_H_ 1

/*
 * The followings are from "ieee754.h".
 * I just changed the name 
 * 	from			to
 * ieee754_float	ieee754_binary32
 * ieee754_double	ieee754_binary64
 * ieee854_long_double	ieee854_binary80
 *
 */


#ifdef WIN32
# ifndef __BYTE_ORDER
#  define __LITTLE_ENDIAN 1234
#  define __BIG_ENDIAN    4321
#  define __BYTE_ORDER    __LITTLE_ENDIAN
# endif
#else /* NOT WIN32 */
#include <endian.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif


union ieee754_binary32
{
    float f;

    /* This is the IEEE 754 single-precision format.  */
    struct
    {
#if	__BYTE_ORDER == __BIG_ENDIAN
	unsigned int negative:1;
	unsigned int exponent:8;
	unsigned int mantissa:23;
#endif				/* Big endian.  */
#if	__BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int mantissa:23;
	unsigned int exponent:8;
	unsigned int negative:1;
#endif				/* Little endian.  */
    } ieee;

    /* This format makes it easier to see if a NaN is a signalling NaN.  */
    struct
    {
#if	__BYTE_ORDER == __BIG_ENDIAN
	unsigned int negative:1;
	unsigned int exponent:8;
	unsigned int quiet_nan:1;
	unsigned int mantissa:22;
#endif				/* Big endian.  */
#if	__BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int mantissa:22;
	unsigned int quiet_nan:1;
	unsigned int exponent:8;
	unsigned int negative:1;
#endif				/* Little endian.  */
    } ieee_nan;
};

#define IEEE754_BINARY32_BIAS	0x7f /* Added to exponent.  */


union ieee754_binary64
{
    double d;

    /* This is the IEEE 754 double-precision format.  */
    struct
    {
#if	__BYTE_ORDER == __BIG_ENDIAN
	unsigned int negative:1;
	unsigned int exponent:11;
	/* Together these comprise the mantissa.  */
	unsigned int mantissa0:20;
	unsigned int mantissa1:32;
#endif				/* Big endian.  */
#if	__BYTE_ORDER == __LITTLE_ENDIAN
# if 	defined(__FLOAT_WORD_ORDER) && __FLOAT_WORD_ORDER == __BIG_ENDIAN
	unsigned int mantissa0:20;
	unsigned int exponent:11;
	unsigned int negative:1;
	unsigned int mantissa1:32;
# else
	/* Together these comprise the mantissa.  */
	unsigned int mantissa1:32;
	unsigned int mantissa0:20;
	unsigned int exponent:11;
	unsigned int negative:1;
# endif
#endif				/* Little endian.  */
    } ieee;

    /* This format makes it easier to see if a NaN is a signalling NaN.  */
    struct
    {
#if	__BYTE_ORDER == __BIG_ENDIAN
	unsigned int negative:1;
	unsigned int exponent:11;
	unsigned int quiet_nan:1;
	/* Together these comprise the mantissa.  */
	unsigned int mantissa0:19;
	unsigned int mantissa1:32;
#else
# if 	defined(__FLOAT_WORD_ORDER) && __FLOAT_WORD_ORDER == __BIG_ENDIAN
	unsigned int mantissa0:19;
	unsigned int quiet_nan:1;
	unsigned int exponent:11;
	unsigned int negative:1;
	unsigned int mantissa1:32;
# else
	/* Together these comprise the mantissa.  */
	unsigned int mantissa1:32;
	unsigned int mantissa0:19;
	unsigned int quiet_nan:1;
	unsigned int exponent:11;
	unsigned int negative:1;
# endif
#endif
    } ieee_nan;
};

#define IEEE754_BINARY64_BIAS	0x3ff /* Added to exponent.  */


union ieee854_binary80
{
    long double d;

    /* This is the IEEE 854 double-extended-precision format.  */
    struct
    {
#if	__BYTE_ORDER == __BIG_ENDIAN
	unsigned int negative:1;
	unsigned int exponent:15;
	unsigned int empty:16;
	unsigned int mantissa0:32;
	unsigned int mantissa1:32;
#endif
#if	__BYTE_ORDER == __LITTLE_ENDIAN
# if 	defined(__FLOAT_WORD_ORDER) && __FLOAT_WORD_ORDER == __BIG_ENDIAN
	unsigned int exponent:15;
	unsigned int negative:1;
	unsigned int empty:16;
	unsigned int mantissa0:32;
	unsigned int mantissa1:32;
# else
	unsigned int mantissa1:32;
	unsigned int mantissa0:32;
	unsigned int exponent:15;
	unsigned int negative:1;
	unsigned int empty:16;
# endif
#endif
    } ieee;

    /* This is for NaNs in the IEEE 854 double-extended-precision format.  */
    struct
    {
#if	__BYTE_ORDER == __BIG_ENDIAN
	unsigned int negative:1;
	unsigned int exponent:15;
	unsigned int empty:16;
	unsigned int one:1;
	unsigned int quiet_nan:1;
	unsigned int mantissa0:30;
	unsigned int mantissa1:32;
#endif
#if	__BYTE_ORDER == __LITTLE_ENDIAN
# if 	defined(__FLOAT_WORD_ORDER) && __FLOAT_WORD_ORDER == __BIG_ENDIAN
	unsigned int exponent:15;
	unsigned int negative:1;
	unsigned int empty:16;
	unsigned int mantissa0:30;
	unsigned int quiet_nan:1;
	unsigned int one:1;
	unsigned int mantissa1:32;
# else
	unsigned int mantissa1:32;
	unsigned int mantissa0:30;
	unsigned int quiet_nan:1;
	unsigned int one:1;
	unsigned int exponent:15;
	unsigned int negative:1;
	unsigned int empty:16;
# endif
#endif
    } ieee_nan;
};

#define IEEE854_BINARY80_BIAS 0x3fff


#ifdef __cplusplus
}
#endif

#endif
