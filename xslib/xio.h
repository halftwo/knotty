/* $Id: xio.h,v 1.15 2015/05/13 09:19:08 gremlin Exp $ */
#ifndef XIO_H_
#define XIO_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* The xio_XXX_function functions should behave just like following 
   functions in <unistd.h>:
	ssize_t read(int fd, void *buf, size_t count);
	ssize_t write(int fd, const void *buf, size_t count);
	off_t lseek(int fildes, off_t offset, int whence);
	int close(int fd);

   NOTE: xio_seek_function()'s second argument is an input-output 
   argument, the output value is the result position after seek operation.
   The function returns 0 (success) or -1 (error).
 */
typedef ssize_t (*xio_read_function)(void *cookie, void *data, size_t size);
typedef ssize_t (*xio_write_function)(void *cookie, const void *data, size_t size);
typedef	int 	(*xio_seek_function)(void *cookie, int64_t *position, int whence);
typedef	int 	(*xio_close_function)(void *cookie);


typedef struct xio_t xio_t;
struct xio_t
{
	xio_read_function 	read;
	xio_write_function 	write;
	xio_seek_function	seek;		/* May be NULL */
	xio_close_function 	close;		/* May be NULL */
};


					/* cookie 		{ R W S C } */
extern const xio_t null_xio;		/* NULL 		{ - - - - } */
extern const xio_t eagain_xio;		/* NULL 		{ R W - - } */
extern const xio_t stdio_xio;		/* FILE * 		{ R W S C } */
extern const xio_t fd_xio;		/* int			{ R W S C } */
extern const xio_t pptr_xio;		/* char **		{ R W - - } */
extern const xio_t zero_xio;		/* size_t * 		{ R W - - } */


#ifdef __cplusplus
}
#endif



#ifdef __cplusplus
					/* cookie 		{ R W S C } */
extern const xio_t istream_xio;		/* std::istream *	{ R - S - } */
extern const xio_t ostream_xio;		/* std::ostream *	{ - W S - } */

#endif /* __cplusplus */


#endif
