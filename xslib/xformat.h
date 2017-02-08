/* $Id: xformat.h,v 1.24 2012/09/20 03:21:47 jiagui Exp $ */
#ifndef XFORMAT_H_
#define XFORMAT_H_

#include "xsdef.h"
#include "xstr.h"
#include "xio.h"
#include "iobuf.h"
#include <stdlib.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct 
{
	/* Flags */
	unsigned int fl_alt:1;
	unsigned int fl_left:1;
	unsigned int fl_space:1;
	unsigned int fl_showsign:1;
	unsigned int fl_padzero:1;
	unsigned int fl_group:1;
	unsigned int fl_outdigits:1;

	int width;		/* Width of output; 0 means none specified.  */
	int precision;		/* Precision of output; negative means none specified.  */

	/* Length modifier */
	unsigned int is_longlong:1;
	unsigned int is_long:1;
	unsigned int is_char:1;
	unsigned int is_short:1;

	int num;		/* The number of characters written so far. */
	int specifier; 		/* Conversion specifier */

	xstr_t spec;		/* The whole specification string begin with % character. */
	xstr_t ext;		/* The extension string */
} xfmt_spec_t;


/*
   When specifier in the format specification is 'p' followed by "{>", the 
   string up to the "<}" in the format string is taken as extension, an 
   argument (must be pointer, because of the 'p' specifier) is consumed,
   and the xfmt_callback_function is called.  The extension string should
   not contain '%'.

   The callback function should return 0 or positive number for known 
   extension.  If the extension is unknown, a negative number should be 
   returned, and no data should be written to the iobuf_t.
   The callback function need not detect the iobuf_t errors, the iobuf_t 
   errors will be detected by xformat() function itself.

   NB: @p may be NULL, special care should be taken of.
 */
typedef int (*xfmt_callback_function)(iobuf_t *ob, const xfmt_spec_t *spec, void *p);



/* Return the number of characters printed (not including the trailing '\0')
   or should be printed if enough space is available.
   If @xio_write is NULL, the printed string is left in @buf and the string
   is NOT null-terminated. If the length of the string is greater than @size,
   only the first @size bytes is left in @buf.
   If @xio_write is NOT NULL, the printed string will be written to 
   @xio_cookie through @xio_write function, and the content of @buf is 
   undefined.
   If @xio_write is (xio_write_function)-1, the print process will stop as
   soon as the @buf is filled up. And the return number is the actual number
   of characters printed or @size if the process is stoped midway.
   NB: The string in the @buf is NOT null-terminated, that is, no '\0'
   is appended or put at the end of string in the @buf.
*/
ssize_t xformat(xfmt_callback_function callback/*NULL*/,
		xio_write_function xio_write/*NULL*/, void *xio_cookie/*NULL*/,
		char *buf, size_t size, const char *fmt, ...) XS_C_PRINTF(6, 7);


ssize_t vxformat(xfmt_callback_function callback/*NULL*/,
		xio_write_function xio_write/*NULL*/, void *xio_cookie/*NULL*/,
		char *buf, size_t size, const char *fmt, va_list ap) XS_C_PRINTF(6, 0);



/* Almost same as xformat(callback, NULL, NULL, buf, max, fmt, ...) except 
   that xfmt_snprintf() always terminates @buf with '\0' if @max > 0.
   This function is very like snprintf() in many aspects except xfmt_snprintf()
   recognize the extension specifier and consume argument (must be a pointer).
 */
ssize_t xfmt_snprintf(xfmt_callback_function callback,
		char *buf, size_t max, const char *fmt, ...) XS_C_PRINTF(4, 5);

/* Almost same as vxformat(callback, NULL, NULL, buf, max, fmt, ap) except 
   that xfmt_vsnprintf() always terminates @buf with '\0' if @max > 0.
 */
ssize_t xfmt_vsnprintf(xfmt_callback_function callback,
		char *buf, size_t max, const char *fmt, va_list ap) XS_C_PRINTF(4, 0);



ssize_t xfmt_sprintf(xfmt_callback_function callback,
		char *buf, const char *fmt, ...) XS_C_PRINTF(3, 4);

ssize_t xfmt_vsprintf(xfmt_callback_function callback,
		char *buf, const char *fmt, va_list ap) XS_C_PRINTF(3, 0);



ssize_t xfmt_iobuf_printf(xfmt_callback_function callback,
		iobuf_t *ob, const char *fmt, ...) XS_C_PRINTF(3, 4);

ssize_t xfmt_iobuf_vprintf(xfmt_callback_function callback,
		iobuf_t *ob, const char *fmt, va_list ap) XS_C_PRINTF(3, 0);


#ifdef __cplusplus
}
#endif

#endif
