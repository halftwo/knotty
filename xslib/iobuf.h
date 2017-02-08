/* $Id: iobuf.h,v 1.15 2015/05/13 09:19:08 gremlin Exp $ */
#ifndef IOBUF_H_
#define IOBUF_H_

#include "xsdef.h"
#include "xio.h"
#include "xstr.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct iobuf_t iobuf_t;


struct iobuf_t
{
	const xio_t		*xio;
	void			*cookie;
	unsigned char		*buf;
	int 			size;
	int 			cur;
	int 			len;
	short 			err;
	unsigned short 		rd:1;
	unsigned short 		wr:1;
	unsigned short		benable:1;
	unsigned short		bstate:1;
	unsigned short 		allocated:1;
};


#define IOBUF_INIT(xio, cookie, buf, size)	{ (xio), (cookie), (buf), (size) }

/* 
    0 Success
   -1 Error
 */

void iobuf_init(iobuf_t *iob, const xio_t *xio, void *cookie, unsigned char *buf, size_t size);

int iobuf_finish(iobuf_t *iob);

int iobuf_seek(iobuf_t *iob, int64_t off, int whence);

int64_t iobuf_tell(iobuf_t *iob);



static inline void iobuf_bstate_enable(iobuf_t *iob) 	{ iob->benable = 1; }
static inline void iobuf_bstate_disable(iobuf_t *iob) 	{ iob->benable = 0; }

static inline bool iobuf_bstate_test(iobuf_t *iob) 	{ return iob->bstate; }
static inline void iobuf_bstate_clear(iobuf_t *iob) 	{ iob->bstate = 0; }



/*
   >0  Success
    0  EAGAIN		Would block	
   -1  			Error
   -2  			End-of-file
   -3  ENOTSUP		The operation is not supported	
   -4  ERANGE		No @delim character found in iob->size length of buffer.
   -5  EINVAL		Argument is invalid (negative or too large)
 */

ssize_t iobuf_read(iobuf_t *iob, void *ptr, size_t n);

ssize_t iobuf_peek(iobuf_t *iob, size_t n, char **p_result/*NULL*/);

ssize_t iobuf_getdelim(iobuf_t *iob, char **p_result/*NULL*/, char delim, bool readit);

ssize_t iobuf_getdelim_xstr(iobuf_t *iob, xstr_t *xstr/*NULL*/, char delim, bool readit);

ssize_t iobuf_getline(iobuf_t *iob, char **p_line/*NULL*/);

ssize_t iobuf_getline_xstr(iobuf_t *iob, xstr_t *line/*NULL*/);

ssize_t iobuf_skip(iobuf_t *iob, ssize_t n);



ssize_t iobuf_write(iobuf_t *iob, const void *ptr, size_t n);

ssize_t iobuf_puts(iobuf_t *iob, const char *str);

ssize_t iobuf_pad(iobuf_t *iob, char c, size_t n);

ssize_t iobuf_putc(iobuf_t *iob, char c);

ssize_t iobuf_printf(iobuf_t *iob, const char *fmt, ...) XS_C_PRINTF(2, 3);

ssize_t iobuf_vprintf(iobuf_t *iob, const char *fmt, va_list ap) XS_C_PRINTF(2, 0);

ssize_t iobuf_flush(iobuf_t *iob);

ssize_t iobuf_reserve(iobuf_t *iob, size_t n, char **p_result/*NULL*/);



#ifdef __cplusplus
}
#endif



#ifdef __cplusplus

#include <stdio.h>
#include <iostream>

inline iobuf_t make_iobuf(FILE *fp, unsigned char *buf, size_t size)
{
	iobuf_t iob = IOBUF_INIT(&stdio_xio, fp, buf, (int)size);
	return iob;
}

inline iobuf_t make_iobuf(int fd, unsigned char *buf, size_t size)
{
	iobuf_t iob = IOBUF_INIT(&fd_xio, (void *)(intptr_t)fd, buf, (int)size);
	return iob;
}

inline iobuf_t make_iobuf(char **pp, unsigned char *buf, size_t size)
{
	iobuf_t iob = IOBUF_INIT(&pptr_xio, pp, buf, (int)size);
	return iob;
}

inline iobuf_t make_iobuf(size_t *pos, unsigned char *buf, size_t size)
{
	iobuf_t iob = IOBUF_INIT(&zero_xio, pos, buf, (int)size);
	return iob;
}

inline iobuf_t make_iobuf(std::ostream& osm, unsigned char *buf, size_t size)
{
	iobuf_t ob = IOBUF_INIT(&ostream_xio, (std::ostream*)&osm, buf, (int)size);
	return ob;
}

inline iobuf_t make_iobuf(std::istream& ism, unsigned char *buf, size_t size)
{
	iobuf_t ib = IOBUF_INIT(&istream_xio, (std::istream*)&ism, buf, (int)size);
	return ib;
}

#endif /* __cplusplus */


#endif
