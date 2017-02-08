#include "xio.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef WIN32
# include <basetsd.h>
  typedef SSIZE_T off_t;
#else
# include <unistd.h>
#endif

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xio.c,v 1.14 2015/05/13 09:19:08 gremlin Exp $";
#endif


const xio_t null_xio;


static ssize_t _eagain_read(void *cookie, void *data, size_t size)
{
	errno = EAGAIN;
	return -1;
}

static ssize_t _eagain_write(void *cookie, const void *data, size_t size)
{
	errno = EAGAIN;
	return -1;
}

const xio_t eagain_xio = {
	_eagain_read,
	_eagain_write,
	NULL,
	NULL,
};


static ssize_t _stdio_read(FILE *fp, void *data, size_t size)
{
	return fread(data, 1, size, fp);
}

static ssize_t _stdio_write(FILE *fp, const void *data, size_t size)
{
	return fwrite(data, 1, size, fp);
}

static int _stdio_seek(FILE *fp, int64_t *position, int whence)
{
	off_t offset = *position;
	int r = fseeko(fp, offset, whence);
	if (r < 0)
		return r;
	offset = ftello(fp);
	if (offset == -1)
		return -1;
	*position = offset;
	return 0;
}

static int _stdio_close(FILE *fp)
{
	return fclose(fp);
}

const xio_t stdio_xio = {
	(xio_read_function)_stdio_read,
	(xio_write_function)_stdio_write,
	(xio_seek_function)_stdio_seek,
	(xio_close_function)_stdio_close,
};


static ssize_t _fd_read(void *cookie, void *data, size_t size)
{
	int fd = (intptr_t)cookie;
	return read(fd, data, size);
}

static ssize_t _fd_write(void *cookie, const void *data, size_t size)
{
	int fd = (intptr_t)cookie;
	return write(fd, data, size);
}

static int _fd_seek(void *cookie, int64_t *position, int whence)
{
	int fd = (intptr_t)cookie;
	off_t offset = lseek(fd, (off_t)*position, whence);
	if (offset == (off_t)-1)
		return -1;
	*position = offset;
	return 0;
}

static int _fd_close(void *cookie)
{
	int fd = (intptr_t)cookie;
	return close(fd);
}

const xio_t fd_xio = {
	_fd_read,
	_fd_write,
	_fd_seek,
	_fd_close,
};


static ssize_t _pptr_read(void *cookie, void *data, size_t size)
{
	char **pp = (char **)cookie;

	if (size > 1)
		memcpy(data, *pp, size);
	else if (size == 1)
		*(char *)data = **pp;

	*pp += size;
	return size;
}

static ssize_t _pptr_write(void *cookie, const void *data, size_t size)
{
	char **pp = (char **)cookie;

	if (size > 1)
		memcpy(*pp, data, size);
	else if (size == 1)
		**pp = *(char *)data;

	*pp += size;
	return size;
}

const xio_t pptr_xio = {
	_pptr_read,
	_pptr_write,
	NULL,
	NULL,
};


static ssize_t _zero_read(void *cookie, void *data, size_t size)
{
	size_t *pos = (size_t *)cookie;

	if (size > 1)
		memset(data, 0, size);
	else if (size == 1)
		*(char *)data = 0;

	if (pos)
		*pos += size;
	return size;
}

static ssize_t _zero_write(void *cookie, const void *data, size_t size)
{
	size_t *pos = (size_t *)cookie;

	if (pos)
		*pos += size;
	return size;
}

const xio_t zero_xio = {
	_zero_read,
	_zero_write,
	NULL,
	NULL,
};

