/* $Id: escape.h,v 1.4 2012/09/20 03:21:47 jiagui Exp $ */
#ifndef ESCAPE_H_
#define ESCAPE_H_ 

#include "xstr.h"
#include "xio.h"
#include "iobuf.h"

#ifdef __cplusplus
extern "C" {
#endif



/* Return positive number if the character is escaped, 
   Return 0 if the character is not escaped.
   Return negative number on error.
 */
typedef int (*escape_callback_function)(xio_write_function xio_write, void *xio_cookie, unsigned char ch);


/* Return 0 for success.
   A negative number on error.
 */
int escape_mem(xio_write_function xio_write, void *xio_cookie,
		const bset_t *meta, escape_callback_function callback,
		const void *data, size_t size);

int escape_xstr(xio_write_function xio_write, void *xio_cookie,
		const bset_t *meta, escape_callback_function callback,
		const xstr_t *xs);

int escape_cstr(xio_write_function xio_write, void *xio_cookie,
		const bset_t *meta, escape_callback_function callback,
		const char *str);



/* Return number of consumed characters.
   Return negative number on error.
   If left < 0, str is nul-terminated.
 */
typedef int (*unescape_callback_function)(xio_write_function xio_write, void *xio_cookie,
					const unsigned char *str, ssize_t left);


/* Return 0 for success.
   A negative number on error.
 */
int unescape_mem(xio_write_function xio_write, void *xio_cookie,
		char escape, unescape_callback_function callback,
		const void *data, size_t size);

int unescape_xstr(xio_write_function xio_write, void *xio_cookie,
		char escape, unescape_callback_function callback,
		const xstr_t *xs);

int unescape_cstr(xio_write_function xio_write, void *xio_cookie,
		char escape, unescape_callback_function callback,
		const char *str);



/* NB: The meta should contains backslash itself. 
   Return the number of bytes write to iobuf_t ob,
   or a negative number if iobuf_write() return negative.
 */
ssize_t backslash_escape_xstr(iobuf_t *ob, const xstr_t *str, const xstr_t *meta,
			const xstr_t *subst_array/*NULL*/, size_t subst_size/*0*/);



#ifdef __cplusplus
}
#endif

#endif
