/* $Id: hex.h,v 1.5 2009/05/31 10:50:19 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2007-03-23
 */
#ifndef hex_h_
#define hex_h_

#include "xsdef.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Return the number of character encoded in 'dst', not including the 
 * tailing '\0'. The encoded string in 'dst' is lower-cased.
 */
ssize_t hexlify(char *dst, const void *src, size_t size);
ssize_t hexlify_nz(char *dst, const void *src, size_t size);


/* This function is the same as hexlify() except its encoded string 
 * in 'dst' is upper-cased.
 */
ssize_t hexlify_upper(char *dst, const void *src, size_t size);
ssize_t hexlify_upper_nz(char *dst, const void *src, size_t size);


/* Return the number of bytes decoded into 'dst'.
 * On error, return a negative number, whose absolute value equals to
 * the consumed number of character in the 'src'.
 * If 'size' is -1, size is taken as strlen(src).
 */
ssize_t unhexlify(void *dst, const char *src, size_t size);

ssize_t unhexlify_ignore_space(void *dst, const char *src, size_t size);


#ifdef __cplusplus
}
#endif

#endif
