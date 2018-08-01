#ifndef DLOG_H_
#define DLOG_H_

#include "xslib/xsdef.h"
#include "xslib/xformat.h"
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


		/* Always print to stderr in addition to write to dlogd. */
#define DLOG_STDERR	0x01

		/* If failed to connect dlogd, print to stderr. */
#define DLOG_PERROR	0x02

		/* Use TCP instead of UDP to connect dlogd server. */
#define DLOG_TCP	0x04


/* Set the identity of the program.  The /option/ argument is an OR of
   any of DLOG_PFILE, DLOG_PERROR. 
   This function should be called once from the begining of the progam.
   if identity is NULL, the dlog identity will be unchanged.
 */
void dlog_set(const char *identity /*NULL*/, int option);

const char *dlog_identity();

int dlog_option();

void dlog_set_option(int option);

void dlog_set_xformat(xfmt_callback_function xfmt_cb);

char *dlog_local_time_str(time_t t, char buf[]);

char *dlog_utc_time_str(time_t t, char buf[]);

/*
   If the callback function return non-zero, do NOT write the log to dlogd.
   The callback must NOT call dlog, or it will result a deadlock.
 */
typedef int (*dlog_callback_function)(void *state, const char *str, size_t length);

void dlog_set_callback(dlog_callback_function cb, void *state);


void zdlog(const xstr_t *identity, const xstr_t *tag, const xstr_t *locus,
		const xstr_t *content);

void vxdlog(xfmt_callback_function xfmt,
		const char *identity, const char *tag, const char *locus,
		const char *format, va_list ap) XS_C_PRINTF(5, 0);

void xdlog(xfmt_callback_function xfmt,
		const char *identity, const char *tag, const char *locus,
		const char *format, ...) XS_C_PRINTF(5, 6);


#define dlog(tag, ...)		xdlog(NULL, NULL, tag, XS_FILE_LINE, __VA_ARGS__)
#define edlog(xfmt, tag, ...)	xdlog(xfmt, NULL, tag, XS_FILE_LINE, __VA_ARGS__)


#ifdef __cplusplus
}
#endif

#endif

