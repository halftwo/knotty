/* $Id: iconv_convert.h,v 1.3 2012/04/10 04:09:53 jiagui Exp $ */
#ifndef ICONV_CONVERT_H_
#define ICONV_CONVERT_H_

#include "xio.h"
#include <iconv.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
   Return 0 on success, or -1 on error. 
   *out should be a malloc()ed buffer or NULL,
   *out_size be the size of *out if *out is not NULL.
   The caller should free() the *out after usage.
*/
int iconv_convert(iconv_t cd, const char *in, size_t in_size, size_t *in_used/*NULL*/,
			char **out, size_t *out_size, size_t *out_used/*NULL*/);


/* Return 0 on success, or -1 on error.
 */
int iconv_write(iconv_t cd, const char *in, size_t in_size, size_t *in_used/*NULL*/,
			xio_write_function xwrite, void *wcookie);


/* Return 0 on success, or -1 on error.
 */
int iconv_pipe(iconv_t cd, xio_read_function xread, void *rcookie, size_t *in_used/*NULL*/,
			xio_write_function xwrite, void *wcookie);



#ifdef __cplusplus
}
#endif

#endif
