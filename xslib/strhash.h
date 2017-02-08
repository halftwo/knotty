/* $Id: strhash.h,v 1.3 2009/02/09 04:46:59 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-06-26
 */
#ifndef STRHASH_H_
#define STRHASH_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


unsigned int strhash(const char *str, unsigned int initval);

unsigned int memhash(const void *mem, size_t n, unsigned int initval);


#ifdef __cplusplus
}
#endif

#endif

