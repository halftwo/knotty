/* $Id: xlog.h,v 1.15 2013/11/28 02:59:40 gremlin Exp $ */
/* 
   Author: XIONG Jiagui
   Date: 2005-09-16
 */
#ifndef XLOG_H_
#define XLOG_H_ 1

#include "xsdef.h"
#include "xformat.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


/* The xlog levels are the same as those in syslog.h
 */
enum {
	XLOG_EMERG 	= 0,  /* system is unusable               */
	XLOG_ALERT 	= 1,  /* action must be taken immediately */
	XLOG_CRIT 	= 2,  /* critical conditions              */
	XLOG_ERR 	= 3,  /* error conditions                 */
        XLOG_WARNING  	= 4,  /* warning conditions               */
        XLOG_NOTICE   	= 5,  /* normal but significant condition */
        XLOG_INFO     	= 6,  /* informational                    */
        XLOG_DEBUG    	= 7,  /* debug-level messages             */

	XLOG_VERBOSE	= 8,  /* logs may be very messy           */
	XLOG_NONSENSE	= 9,  /* don't use this                   */
};


extern int xlog_level;


/* NOTE: buf is ended with '\n'. */
typedef void (*xlog_write_function)(int level, const char *locus, const char *buf, size_t size);


/* Return the old writer.
 */
xlog_write_function xlog_set_writer(xlog_write_function writer);


/* This is the default writer.
   You may want to call this function in your own xlog_writer_function.
 */
void xlog_default_writer(int level, const char *locus, const char *buf, size_t size);


/*
	void xlog(int level, const char *fmt, ...);
	void exlog(int level, xfmt_callback_function cb, const char *fmt, ...);
 */
#ifdef NDEBUG

#define xlog(LEVEL, ...)	((void)0)

#define exlog(LEVEL, ...)	((void)0)

#else

#define xlog(LEVEL, ...)						\
do { 									\
	if ((LEVEL) <= xlog_level)					\
		_xlog((LEVEL), XS_FILE_LINE, NULL, __VA_ARGS__);	\
} while(0)

#define exlog(LEVEL, XFMT_CB, ...)					\
do {									\
	if ((LEVEL) <= xlog_level)					\
		_xlog((LEVEL), XS_FILE_LINE, (XFMT_CB), __VA_ARGS__);	\
} while(0)

#endif



void _xlog(int level, const char *locus, xfmt_callback_function cb, const char *fmt, ...) XS_C_PRINTF(4, 5);

void _vxlog(int level, const char *locus, xfmt_callback_function cb, const char *fmt, va_list ap) XS_C_PRINTF(4, 0);


#ifdef __cplusplus
}               
#endif
	
#endif  

