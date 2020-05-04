/* 
   Author: XIONG Jiagui
   Date: 2005-09-16
 */
#define _BSD_SOURCE /* or _SVID_SOURCE or _GNU_SOURCE */
#include "xlog.h"
#include "xformat.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xlog.c,v 1.17 2012/09/20 03:21:47 jiagui Exp $";
#endif

int xlog_level;

void xlog_default_writer(int level, const char *locus, const char *buf, size_t size)
{
	char prefix[128];
	size_t len;

	{
		struct timeval tv;
		struct tm tm;
		gettimeofday(&tv, NULL);
		localtime_r(&tv.tv_sec, &tm);

		len = xfmt_snprintf(NULL, prefix, sizeof(prefix), "%02d%02d%02d%c%02d%02d%02d.%03d %d %s",
			tm.tm_year - 100, tm.tm_mon + 1, tm.tm_mday,
			"umtwrfsu"[tm.tm_wday],
			tm.tm_hour, tm.tm_min, tm.tm_sec, (int)tv.tv_usec / 1000,
			level, locus);
	}

	if (len > sizeof(prefix) - 2)
		len = sizeof(prefix) - 2;

	prefix[len++] = ':';
	prefix[len++] = '\t';

	{
		struct iovec iov[2];
		iov[0].iov_base = prefix;
		iov[0].iov_len = len;
		iov[1].iov_base = (char *)buf;
		iov[1].iov_len = size;
		writev(STDERR_FILENO, iov, 2);
	}
}

static xlog_write_function _current_writer = xlog_default_writer;

xlog_write_function xlog_set_writer(xlog_write_function writer)
{
	xlog_write_function old = _current_writer;
	_current_writer = writer ? writer : xlog_default_writer;
	return old;
}

void _xlog(int level, const char *locus, xfmt_callback_function cb, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_vxlog(level, locus, cb, fmt, ap);
	va_end(ap);
}

void _vxlog(int level, const char *locus, xfmt_callback_function cb, const char *fmt, va_list ap)
{
	char buf[1024];
	xlog_write_function writer = _current_writer;
	size_t len = xfmt_vsnprintf(cb, buf, sizeof(buf), fmt, ap);

	if (len > sizeof(buf) - 1)
		len = sizeof(buf) - 1;

	if (buf[len-1] != '\n')
	{
		if (len < sizeof(buf) - 1)
		{
			buf[len++] = '\n';
			buf[len] = 0;
		}
		else
			buf[len-1] = '\n';
	}

	writer(level, locus, buf, len);
}

