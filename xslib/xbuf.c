#include "xbuf.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xbuf.c,v 1.3 2015/05/13 03:43:21 gremlin Exp $";
#endif


ssize_t xbuf_vprintf(xbuf_t *xb, const char *fmt, va_list ap)
{
	ssize_t n = 0;
	ssize_t avail = xb->capacity - xb->len;
	if (avail > 0)
	{
		n = vsnprintf((char *)xb->data + xb->len, avail, fmt, ap);
		if (n > avail)
			n = avail;
		xb->len += n;
	}
	return n;
}

ssize_t xbuf_printf(xbuf_t *xb, const char *fmt, ...)
{
	ssize_t len;
	va_list ap;

	va_start(ap, fmt);
	len = xbuf_vprintf(xb, fmt, ap);
	va_end(ap);
	return len;
}

ssize_t xbuf_write(xbuf_t *xb, const void *buf, size_t size)
{
	size_t avail = xb->capacity - xb->len;
	if (size > avail)
		size = avail;

	if (size > 1)
		memcpy(xb->data + xb->len, buf, size);
	else if (size == 1)
		xb->data[xb->len] = *(unsigned char *)buf;

	xb->len += size;
	return size;
}

ssize_t xbuf_puts(xbuf_t *xb, const char *str)
{
	size_t avail = xb->capacity - xb->len;
	size_t n = 0;

	while (avail && *str)
	{
		xb->data[xb->len++] = *str++;
		--avail;
		++n;
	}

	return n;
}

ssize_t xbuf_putc(xbuf_t *xb, int ch)
{
	size_t avail = xb->capacity - xb->len;
	if (avail)
	{
		xb->data[xb->len++] = ch;
		return 1;
	}
	return 0;
}

ssize_t xbuf_read(xbuf_t *xb, void *data, size_t size)
{
	size_t avail = xb->capacity - xb->len;
	if (size > avail)
		size = avail;

	if (size > 1)
		memcpy(data, xb->data + xb->len, size);
	else if (size == 1)
		*(unsigned char *)data = xb->data[xb->len];

	xb->len += size;
	return size;
}

int xbuf_getc(xbuf_t *xb)
{
	size_t avail = xb->capacity - xb->len;
	if (avail)
	{
		return (unsigned char)xb->data[xb->len++];
	}
	return -1;
}

int xbuf_seek(xbuf_t *xb, int64_t *position, int whence)
{
	ssize_t offset = *position;
	if (whence == SEEK_SET)
	{
		xb->len = offset;
	}
	else if (whence == SEEK_CUR)
	{
		xb->len += offset;
	}
	else if (whence == SEEK_END)
	{
		xb->len = xb->capacity + offset;
	}
	else
	{
		return -1;
	}

	if ((ssize_t)xb->len < 0)
		xb->len = 0;
	else if (xb->len > xb->capacity)
		xb->len = xb->capacity;

	*position = xb->len;
	return 0;
}


const xio_t xbuf_xio = {
	(xio_read_function) xbuf_read,
	(xio_write_function) xbuf_write,
	(xio_seek_function) xbuf_seek,
	NULL,
};


#ifdef TEST_XBUF

int main(int argc, char **argv)
{
	xbuf_t xb;
	unsigned char buf[10];

	xbuf_init(&xb, buf, sizeof(buf));

	xbuf_printf(&xb, "hello,world!");
	printf("%d=%.*s#\n", xb.len, xb.len, xb.data);

	xbuf_rewind(&xb);

	xbuf_write(&xb, "hello,world!", 12);
	printf("%d=%.*s#\n", xb.len, xb.len, xb.data);

	return 0;
}

#endif

