/* $Id: xbuf.h,v 1.4 2015/05/13 03:43:21 gremlin Exp $ */
#ifndef XBUF_H_
#define XBUF_H_ 1

#include "xsdef.h"
#include "xio.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


extern const xio_t xbuf_xio;		/* xbuf_t *		{ R W S - } */
					/* cookie		{ R W S C } */


typedef struct
{
	unsigned char *data;
	ssize_t len;			/* len <= capacity */
	ssize_t capacity;
} xbuf_t;


#define XBUF_INIT(BUF, CAPACITY)	{ (BUF), 0, (ssize_t)(CAPACITY) }


static inline void xbuf_init(xbuf_t *xb, void *data, size_t capacity)
{
	xb->data = (unsigned char *)data;
	xb->len = 0;
	xb->capacity = (ssize_t)capacity;
}

static inline void xbuf_rewind(xbuf_t *xb)
{
	xb->len = 0;
}


/* xb->data may NOT terminate with '\0' after call
 * xbuf_write(), xbuf_puts() or xbuf_putc().
 */
ssize_t xbuf_write(xbuf_t *xb, const void *data, size_t size);
ssize_t xbuf_puts(xbuf_t *xb, const char *str);
ssize_t xbuf_putc(xbuf_t *xb, int ch);


/* xb->data always terminates with '\0' after call
 * xbuf_printf() or xbuf_vprintf() if xb->capacity > 0.
 */
ssize_t xbuf_vprintf(xbuf_t *xb, const char *fmt, va_list ap) XS_C_PRINTF(2, 0);
ssize_t xbuf_printf(xbuf_t *xb, const char *fmt, ...) XS_C_PRINTF(2, 3);


ssize_t xbuf_read(xbuf_t *xb, void *data, size_t size);
int xbuf_getc(xbuf_t *xb);


int xbuf_seek(xbuf_t *xb, int64_t *position, int whence);



#ifdef __cplusplus
}
#endif

#endif


