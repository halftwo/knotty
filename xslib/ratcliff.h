/* $Id: ratcliff.h,v 1.3 2009/02/09 04:46:59 jiagui Exp $ */
/*
   Compute Ratcliff-Obershelp similarity of two strings.
 */
#ifndef RATCLIFF_H_
#define RATCLIFF_H_ 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
   The returned value is between 0.0 and 1.0.
 */
float ratcliff_similarity(const char *s1, size_t len1, const char *s2, size_t len2);


#ifdef __cplusplus
}
#endif

#endif

