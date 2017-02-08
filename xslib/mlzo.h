/* $Id: mlzo.h,v 1.7 2013/11/13 07:51:58 gremlin Exp $ */
/*
   Some convenient wrapper function for minilzo.
 */
#ifndef MLZO_H_
#define MLZO_H_

#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


#define MLZO_COMPRESS_BOUND(SRCLEN)	((SRCLEN) + (SRCLEN) / 16 + 67)


int mlzo_init();


/* Return the number of bytes encoded,
 * or a negative number on error.
 * The size of out buffer should be at least MLZO_COMPRESS_BOUND(in_len)
 */
int mlzo_compress(const unsigned char *in, int in_len, unsigned char *out);


/* Safe decompression with overrun testing.
 * Return the number of bytes decoded,
 * or a negative number on error.
 */
int mlzo_decompress_safe(const unsigned char *in, int in_len, unsigned char *out, int out_size);


#ifdef __cplusplus
}
#endif

#endif

