/* $Id: strbuf.h,v 1.29 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2005-06-19
 */
#ifndef STRBUF_H_
#define STRBUF_H_ 1

#include "xsdef.h"
#include "xio.h"
#include <stdarg.h>
#include <assert.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


extern const xio_t strbuf_xio;


typedef struct
{
	char *buf;
	int size;
	int real_len;
	int virtual_len;
	unsigned int size_fixed:1;
	unsigned int allocated:1;
} strbuf_t;


/* 0 for success, negative number for error. */
int strbuf_init(strbuf_t *sb, char *buf, int size, bool size_fixed);
void strbuf_finish(strbuf_t *sb);

void strbuf_reset(strbuf_t *sb);
char *strbuf_take(strbuf_t *sb);


int strbuf_vprintf(strbuf_t *sb, const char *fmt, va_list ap) XS_C_PRINTF(2, 0);
int strbuf_printf(strbuf_t *sb, const char *fmt, ...) XS_C_PRINTF(2, 3);
int strbuf_write(strbuf_t *sb, const void *ptr, int size);
int strbuf_puts(strbuf_t *sb, const char *str);
int strbuf_putc(strbuf_t *sb, int ch);
int strbuf_reserve(strbuf_t *sb, int size, char **p_buf);
int strbuf_advance(strbuf_t *sb, int size);

int strbuf_pop(strbuf_t *sb, int size);


#define strbuf_buf(SB)		((SB)->buf)
#define strbuf_size(SB)		((SB)->size)
#define strbuf_rlen(SB)		((SB)->real_len)
#define strbuf_vlen(SB)		((SB)->virtual_len)


#ifdef __cplusplus
}
#endif

#endif

