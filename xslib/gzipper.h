/* $Id: gzipper.h,v 1.1 2013/11/28 13:49:18 gremlin Exp $ */
#ifndef GZIPPER_H_
#define GZIPPER_H_

#include "xsdef.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return the size of compressed data, or negative number on error.
 * The 3rd and 4th arguments are like the 1st and 2nd arguments of
 * library function getline().
 */
ssize_t gzip_compress(const void *in, size_t isize, unsigned char **out, size_t *osize);


/* Return the size of compressed data, or negative number on error.
 */
ssize_t gzip_decompress(const void *in, size_t isize, unsigned char **out, size_t *osize);



#ifdef __cplusplus
}
#endif

#endif
