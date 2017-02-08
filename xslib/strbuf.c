/*
   Author: XIONG Jiagui
   Date: 2005-06-19
 */
#include "strbuf.h"
#include "xformat.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: strbuf.c,v 1.24 2010/11/29 07:08:32 jiagui Exp $";
#endif


#define INCREMENT_SIZE	65536


int strbuf_init(strbuf_t *sb, char *buf, int size, bool size_fixed)
{
	sb->buf = buf;
	sb->size = size;
	assert(size >= 0);
	sb->size_fixed = size_fixed;
	sb->allocated = false;
	if (sb->buf == 0 && sb->size > 0)
	{
		sb->buf = (char *)malloc(sb->size);
		if (sb->buf == 0)
			return -1;
		sb->allocated = true;
	}
	sb->real_len = sb->virtual_len = 0;
	if (sb->size > 0)
		sb->buf[0] = 0;
	return 0;
}

char *strbuf_take(strbuf_t *sb)
{
	if (sb->allocated)
	{
		sb->allocated = false;
		return sb->buf;
	}
	return 0;
}

void strbuf_finish(strbuf_t *sb)
{
	if (sb->allocated)
	{
		free(sb->buf);
		sb->allocated = false;
	}
}

void strbuf_reset(strbuf_t *sb)
{
	sb->real_len = sb->virtual_len = 0;
	if (sb->size > 0)
		sb->buf[0] = 0;
}

static int _grow(strbuf_t *sb, int hint)
{
	int new_size;
	char *buf;

	assert(!sb->size_fixed);
	new_size = sb->size < INCREMENT_SIZE ? sb->size * 2 : sb->size + INCREMENT_SIZE;
	if (new_size < 256)
		new_size = 256;
	if (new_size < hint)
		new_size = hint;

	if (sb->allocated)
		buf = (char *)realloc(sb->buf, new_size);
	else
	{
		buf = (char *)malloc(new_size);
		if (buf)
		{
			sb->allocated = true;
			memcpy(buf, sb->buf, sb->real_len + 1);
		}
	}

	if (buf == NULL)
		return 0;
	sb->buf = buf;
	sb->size = new_size;
	return sb->size;
}

int strbuf_vprintf(strbuf_t *sb, const char *fmt, va_list ap)
{
	int avail, n;
	va_list copied_ap;

	avail = sb->size - sb->real_len;
	assert(avail > 0);

	va_copy(copied_ap, ap);
	n = xfmt_vsnprintf(NULL, &sb->buf[sb->real_len], avail, fmt, copied_ap);
	va_end(copied_ap);

	if (n < avail)
	{
		sb->real_len += n;
	}
	else if (sb->size_fixed)
	{
		sb->real_len = sb->size > 0 ? sb->size - 1 : 0;
	}
	else
	{
		if (!_grow(sb, sb->real_len + n + 1))
			return -1;

		n = xfmt_vsprintf(NULL, &sb->buf[sb->real_len], fmt, ap);
		sb->real_len += n;
	}
	sb->virtual_len += n;
	return sb->virtual_len;
}

int strbuf_printf(strbuf_t *sb, const char *fmt, ...)
{
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = strbuf_vprintf(sb, fmt, ap);
	va_end(ap);
	return len;
}

int strbuf_write(strbuf_t *sb, const void *buf, int size)
{
	if (size > 0)
	{
		int avail = sb->size - sb->real_len;
		assert(avail > 0);
		if (size < avail)
		{
			memcpy(&sb->buf[sb->real_len], buf, size);
			sb->real_len += size;
			sb->buf[sb->real_len] = '\0';
		}
		else if (sb->size_fixed)
		{
			if (avail > 0)
			{
				memcpy(&sb->buf[sb->real_len], buf, avail - 1);
				sb->real_len = sb->size - 1;
				sb->buf[sb->real_len] = '\0';
			}
		}
		else
		{
			if (!_grow(sb, sb->real_len + size + 1))
				return -1;
			memcpy(&sb->buf[sb->real_len], buf, size);
			sb->real_len += size;
			sb->buf[sb->real_len] = '\0';
		}
		sb->virtual_len += size;
	}
	return sb->virtual_len;
}

int strbuf_puts(strbuf_t *sb, const char *str)
{
	int len = strlen(str);
	return strbuf_write(sb, str, len);
}

int strbuf_putc(strbuf_t *sb, int ch)
{
	int avail = sb->size - sb->real_len;
	assert(avail > 0);
	if (1 < avail)
	{
		sb->buf[sb->real_len] = ch;
		sb->real_len++;
		sb->buf[sb->real_len] = '\0';
	}
	else if (!sb->size_fixed)
	{
		if (!_grow(sb, sb->real_len + 1 + 1))
			return -1;
		sb->buf[sb->real_len] = ch;
		sb->real_len++;
		sb->buf[sb->real_len] = '\0';
	}
	sb->virtual_len++;
	return sb->virtual_len;
}

int strbuf_pop(strbuf_t *sb, int size)
{
	if (size > 0)
	{
		if (sb->virtual_len > 0)
		{
			if (sb->virtual_len > size)
				sb->virtual_len -= size;
			else
				sb->virtual_len = 0;

			if (sb->real_len > sb->virtual_len)
			{
				sb->real_len = sb->virtual_len;
				sb->buf[sb->real_len] = 0;
			}
		}
	}
	return sb->virtual_len;
}

int strbuf_reserve(strbuf_t *sb, int size, char **p_buf)
{
	int r;
	int avail = sb->size - 1 - sb->real_len;
	if (size <= 0)
	{
		if (p_buf)
			*p_buf = NULL;
		return size;
	}

	if (size > avail && (sb->size_fixed || !_grow(sb, sb->real_len + size + 1)))
		r = avail;
	else
		r = size;

	if (p_buf)
		*p_buf = sb->buf + sb->real_len;
	return r;
}

int strbuf_advance(strbuf_t *sb, int size)
{
	int avail = sb->size - 1 - sb->real_len;
	if (size <= 0)
		return size;
	if (size > avail)
		return -1;

	sb->real_len += size;
	sb->virtual_len += size;
	return size;
}


static ssize_t _write(strbuf_t *sb, const void *ptr, size_t size)
{
	if (size <= INT_MAX)
	{
		strbuf_write(sb, ptr, size);
		return size;
	}
	return -1;
}

const xio_t strbuf_xio = {
	NULL,
	(xio_write_function) _write,
	NULL,
	NULL,
};


#ifdef TEST_STRBUF

int main(int argc, char **argv)
{
	strbuf_t sb;
	char buf[10];

	strbuf_init((&sb), buf, 10, 1);
	printf("%d:%d:%d: %s#\n", 
		strbuf_rlen(&sb), strbuf_vlen(&sb), strbuf_size(&sb), strbuf_buf(&sb));

	strbuf_printf(&sb, "hello, world!");
	printf("%d:%d:%d: %s#\n", 
		strbuf_rlen(&sb), strbuf_vlen(&sb), strbuf_size(&sb), strbuf_buf(&sb));

	strbuf_reset(&sb);
	printf("%s#\n", strbuf_buf(&sb));

	strbuf_write(&sb, "hello, world!", 12);
	printf("%d:%d:%d: %s#\n", 
		strbuf_rlen(&sb), strbuf_vlen(&sb), strbuf_size(&sb), strbuf_buf(&sb));

	strbuf_finish(&sb);
	return 0;
}

#endif

