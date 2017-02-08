/* 
   Author: XIONG Jiagui
   Date: 2008-04-16
 */
#ifndef ZLOG_H_
#define ZLOG_H_ 1

#include "xsdef.h"
#include <stddef.h>
#include <stdarg.h>
 
#ifdef __cplusplus
extern "C" {
#endif

typedef struct zlog_t zlog_t;

enum
{
	ZLOG_FLUSH_EVERYLOG = 0x01,
	ZLOG_NO_NEWLINE = 0x02,
	ZLOG_AUTO_COMPRESS = 0x04,	/* lz4 compress */
};

zlog_t *zlog_create(const char *prefix, size_t max_size, int flags);

void zlog_destroy(zlog_t *zl);

void zlog_switch(zlog_t *zl);

void zlog_flush(zlog_t *zl);

void zlog_printf(zlog_t *zl, const char *format, ...) XS_C_PRINTF(2, 3);

void zlog_vprintf(zlog_t *zl, const char *format, va_list ap) XS_C_PRINTF(2, 0);

void zlog_write(zlog_t *zl, const char *str, int size);



#ifdef __cplusplus
}               
#endif
	
#endif  

