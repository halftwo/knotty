#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "calltrace.h"
#include "cstr.h"
#include <execinfo.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: calltrace.c,v 1.5 2010/08/06 12:31:11 jiagui Exp $";
#endif

#define STACK_SIZE	128
#define UNWANTED	2

ssize_t calltrace(int level, char *buf, size_t size)
{
	void *stack[STACK_SIZE];
	void **stk = stack;
	char *p = buf, *end = buf + size;
	int i, num;
	char **strs;

	if (size == 0)
		return 0;
	buf[0] = 0;

	if (level < 0)
		level = 0;
	++level;

	num = backtrace(stk, STACK_SIZE) - UNWANTED;
	stk += level;
	num -= level;

	if (num > 0)
	{
		strs = backtrace_symbols(stk, num);
		if (!strs)
			return 0;

		for (i = 0; i < num; ++i)
		{
			p = cstr_pcopy(p, end, strs[i]);
			p = cstr_pputc(p, end, '\n');
		}

		free(strs);
	}
	return p < end ? p - buf : size - 1;
}

ssize_t calltrace_iobuf(int level, iobuf_t *ob)
{
	void *stack[STACK_SIZE];
	void **stk = stack;
	ssize_t r = 0, n = 0;
	int i, num;
	char **strs;

	if (level < 0)
		level = 0;
	++level;

	num = backtrace(stk, STACK_SIZE) - UNWANTED;
	stk += level;
	num -= level;

	if (num > 0)
	{
		strs = backtrace_symbols(stk, num);
		if (!strs)
			return 0;

		for (i = 0; i < num; ++i)
		{
			if ((r = iobuf_puts(ob, strs[i])) < 0)
				goto done;
			n += r;
			if ((r = iobuf_putc(ob, '\n')) < 0)
				goto done;
			n += r;
		}
	done:
		free(strs);
	}
	return r < 0 ? r : n;
}

