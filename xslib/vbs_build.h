/* $Id: vbs_build.h,v 1.4 2015/05/28 06:43:30 gremlin Exp $ */
#ifndef VBS_BUILD_H_
#define VBS_BUILD_H_

#include "xio.h"
#include "vbs_pack.h"
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef int (*vbs_build_callback_function)(vbs_packer_t *job, void *ctx);


/*
 * Specifiers in the format string:
 *
 * Specifier	Consumed Arguments
 * ---------	------------------
 *	@	callback, void *	// callback function and it's ctx, value or item pairs
 * 	i	int			// VBS_INTEGER
 * 	I	intmax_t		// VBS_INTEGER
 * 	t	bool			// VBS_BOOL
 * 	f	double			// VBS_FLOATING
 * 	d	decimal64_t		// VBS_DECIMAL
 * 	s	const char *		// VBS_STRING
 * 	s*	const char *, int	// VBS_STRING
 * 	s#	const char *, ssize_t	// VBS_STRING
 * 	S	const xstr_t *		// VBS_STRING
 * 	b*	const void *, int 	// VBS_BLOB
 * 	b#	const void *, ssize_t 	// VBS_BLOB
 * 	B	const xstr_t *		// VBS_BLOB
 * 	r*	const void *, int	// vbs encoded binary, just copy it, value or item pairs
 * 	r#	const void *, ssize_t	// vbs encoded binary, just copy it, value or item pairs
 * 	R	const xstr_t *		// vbs encoded binary, just copy it, value or item pairs
 * 	l	const vbs_list_t *	// VBS_LIST
 * 	m	const vbs_dict_t *	// VBS_DICT
 * 	x	const vbs_data_t *	// VBS_DATA
 * 	n				// VBS_NULL
 * 	[				// VBS_LIST
 * 	]				// VBS_TAIL
 * 	{				// VBS_DICT
 * 	}				// VBS_TAIL
 * 	^				// separator between key and value
 * 	;				// separator between items
 * ---------	------------------
 *
 * Only integer and string can be the dict key. Spaces are ignored.
 * format example:
 * 	{s^I;s^[f;t];s*^R;s^@;R;s^{@;i^R};}
 *
 */


/* The format could be part of dict or list.
 * You can build the vbs part by part.
 * Return 0 for success.
 * Return negative number for error.
 * You should check job->error and job->depth after the function call.
 */
int vbs_vbuild_format(vbs_packer_t *job, const char *format, va_list ap);

int vbs_build_format(vbs_packer_t *job, const char *format, ...);



/* The format must be integrity. 
 * Return 0 for success.
 * Return negative number for error.
 */
int vbs_vbuild_write(xio_write_function xio_write, void *xio_ctx, const char *format, va_list ap);

int vbs_build_write(xio_write_function xio_write, void *xio_ctx, const char *format, ...);



/* The format must be integrity. 
 * Return the size needed by the encoded vbs binary if buf size is large enough.
 * It can be large than the second argument ~size~.
 * Return a negative number if error.
 */
ssize_t vbs_vbuild_buf(void *buf, size_t size, const char *format, va_list ap);

ssize_t vbs_build_buf(void *buf, size_t size, const char *format, ...);


/* The format must be integrity. 
 * Return the size of the vbs encoded binary.
 * Return a negative number if error.
 */
ssize_t vbs_vbuild_alloc(unsigned char **pbuf, const char *format, va_list ap);

ssize_t vbs_build_alloc(unsigned char **pbuf, const char *format, ...);



#ifdef __cplusplus
}
#endif

#endif
