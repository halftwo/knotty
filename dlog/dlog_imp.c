#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "dlog_imp.h"
#include "xslib/xformat.h"
#include "xslib/xsver.h"
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define PROGRAM program_invocation_short_name
extern char *program_invocation_short_name;

static struct dlog_record _record_prototype = 
{
	 0, 	/* size */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	DLOG_RECORD_VERSION, 0, DLOG_TYPE_RAW, 0, 
#elif __BYTE_ORDER == __BIG_ENDIAN
	0, DLOG_TYPE_RAW, 0, DLOG_RECORD_VERSION,
#else
# error "unsupported endian"
#endif 
};

static inline int _str_max_copy(char *dst, const char *src, size_t max)
{
	char *x = (char *)memccpy(dst, src, 0, max);
	return x ? x - 1 - dst : max;
}

static inline char *_put_identity_tag_locus(struct dlog_record *rec,
				const char *identity, const char *tag, const char *locus)
{
	char *p = rec->str;

	if (identity && identity[0])
	{
		p += _str_max_copy(p, identity, DLOG_IDENTITY_MAX);
	}
	else if (PROGRAM && PROGRAM[0])
	{
		p += _str_max_copy(p, PROGRAM, DLOG_IDENTITY_MAX);
	}
	else
	{
		*p++ = '-';
	}
	*p++ = ' ';

	if (tag && tag[0])
	{
		p += _str_max_copy(p, tag, DLOG_TAG_MAX);
	}
	else
	{
		*p++ = '-';
	}
	*p++ = ' ';

	if (locus && locus[0])
	{
		p += _str_max_copy(p, locus, DLOG_LOCUS_MAX);
	}
	else
	{
		*p++ = '-';
	}
	rec->locus_end = p - rec->str;
	*p++ = ' ';

	return p;
}

static inline int _xstr_max_copy(char *dst, const xstr_t *src, size_t max)
{
	int n = src->len < max ? src->len : max;
	memcpy(dst, src->data, n);
	return n;
}

static inline char *_put2_identity_tag_locus(struct dlog_record *rec,
				const xstr_t *identity, const xstr_t *tag, const xstr_t *locus)
{
	char *p = rec->str;

	if (identity && identity->len)
	{
		p += _xstr_max_copy(p, identity, DLOG_IDENTITY_MAX);
	}
	else if (PROGRAM && PROGRAM[0])
	{
		p += _str_max_copy(p, PROGRAM, DLOG_IDENTITY_MAX);
	}
	else
	{
		*p++ = '-';
	}
	*p++ = ' ';

	if (tag && tag->len)
	{
		p += _xstr_max_copy(p, tag, DLOG_TAG_MAX);
	}
	else
	{
		*p++ = '-';
	}
	*p++ = ' ';

	if (locus && locus->len)
	{
		p += _xstr_max_copy(p, locus, DLOG_LOCUS_MAX);
	}
	else
	{
		*p++ = '-';
	}
	rec->locus_end = p - rec->str;
	*p++ = ' ';

	return p;
}

void dlog_compose(struct dlog_record *rec, const xstr_t *identity, 
		const xstr_t *tag, const xstr_t *locus, const xstr_t *content)
{
	char *buf = (char *)rec;
	char *p = rec->str;
	int n;

	if (!_record_prototype.pid)
		_record_prototype.pid = getpid();

	*rec = _record_prototype;
	p = _put2_identity_tag_locus(rec, identity, tag, locus);
	n = p - buf;

	if (content)
	{
		if (n + content->len < DLOG_RECORD_MAX_SIZE)
		{
			memcpy(p, content->data, content->len);
			n += content->len;
		}
		else
		{
			memcpy(p, content->data, DLOG_RECORD_MAX_SIZE - 1 - n);
			n = DLOG_RECORD_MAX_SIZE - 1;
			rec->truncated = 1;
		}
	}
	
	while (buf[n - 1] == '\r' || buf[n - 1] == '\n')
	{
		--n;
	}
	buf[n] = 0;

	rec->size = n + 1;		/* include the trailing '\0'. */
}

void dlog_vmake(struct dlog_record *rec, xfmt_callback_function callback,
		const char *identity, const char *tag, const char *locus,
		const char *format, va_list ap)
{
	char *buf = (char *)rec;
	char *p = rec->str;
	int n;

	if (!_record_prototype.pid)
		_record_prototype.pid = getpid();

	*rec = _record_prototype;
	p = _put_identity_tag_locus(rec, identity, tag, locus);
	n = p - buf;
	n += vxformat(callback, (xio_write_function)-1, 0, buf + n, DLOG_RECORD_MAX_SIZE - n, format, ap);

	if (n > DLOG_RECORD_MAX_SIZE - 1)
	{
		n = DLOG_RECORD_MAX_SIZE - 1;
		rec->truncated = 1;
	}

	while (buf[n - 1] == '\r' || buf[n - 1] == '\n')
	{
		--n;
	}
	buf[n] = 0;

	rec->size = n + 1;		/* include the trailing '\0'. */
}

void dlog_make(struct dlog_record *rec, xfmt_callback_function callback,
		const char *identity, const char *tag, const char *locus,
		const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	dlog_vmake(rec, callback, identity, tag, locus, format, ap);
	va_end(ap);
}

const char* dlog_version_rcsid()
{
	xslib_version_rcsid();
	return "$dlog: " DLOG_VERSION " $";
}

