/* $Id: xutf8.h,v 1.1 2015/06/15 04:50:41 gremlin Exp $ */
#ifndef XUTF8_H_
#define XUTF8_H_

#include "xstr.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Return number of byte the unicode code point occupied.
 * Return 0 if the string is not encoded as valid utf8.
 */
size_t xutf8_cstr_to_code(const char *s, int *pcode);
size_t xutf8_xstr_to_code(const xstr_t *xs, int *pcode);


/* Return number of byte writen to the buf.
 * Return 0 if the code is not a valid unicode code point.
 */
size_t xutf8_cstr_from_code(char *buf, int code);


/* Return number of unicode code point in the utf8 string.
 */
size_t xutf8_cstr_count(const char *s, char **end/*NULL*/);
size_t xutf8_xstr_count(const xstr_t *s, xstr_t *end/*NULL*/);



#ifdef __cplusplus
}
#endif

#endif
