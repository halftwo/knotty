#include "iobuf.h"
#include "xformat.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: iobuf.c,v 1.12 2012/09/20 03:21:47 jiagui Exp $";
#endif


#define CHECK_ALLOC(iob) 						\
	if (!(iob)->buf && (iob)->size) {				\
		if (_do_alloc(iob) < 0) return -1;			\
	}


#define CHECK_READ(iob)							\
	if (!(iob)->rd) {						\
		CHECK_ALLOC(iob)					\
		if (iob->len) {						\
			_flush_internal(iob);				\
			if (iob->len) return -1;			\
		}							\
		(iob)->wr = 0;						\
		(iob)->rd = 1;						\
	}


#define CHECK_WRITE(iob)						\
	if (!(iob)->wr) {						\
		CHECK_ALLOC(iob)					\
		if (iob->len) return -1;				\
		(iob)->len = 0;						\
		(iob)->cur = 0;						\
		(iob)->rd = 0;						\
		(iob)->wr = 1;						\
	}


#define NOT_SUPPORT(iob) 		do {				\
	(iob)->err = ENOTSUP;						\
	return -3;							\
} while (0)


#define OUT_OF_RANGE(iob)		do {				\
	(iob)->err = ERANGE;						\
	return -4;							\
} while (0)


#define INVALID_ARGUMENT(iob)		do {				\
	(iob)->err = EINVAL;						\
	return -5;							\
} while (0)



static int _do_alloc(iobuf_t *iob)
{
	if (iob->size < 0)
		iob->size = 0;

	if (!iob->buf && iob->size)
	{
		iob->buf = (unsigned char *)malloc(iob->size);
		if (!iob->buf)
		{
			iob->err = errno;
			return -1;
		}
		iob->allocated = 1;
	}

	return 0;
}


/* 0	Would block
   -1	Error
   -2	End of file
 */
static inline ssize_t _do_read(iobuf_t *iob, void *buf, size_t size)
{
	ssize_t n;

	if (!iob->xio->read)
		NOT_SUPPORT(iob);
	if (iob->benable && iob->bstate)
		return 0;
again:
	n = iob->xio->read(iob->cookie, buf, size);
	if (n < 0)
	{
		iob->err = errno;
		switch (iob->err)
		{
		case EINTR:
			goto again;

		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
			iob->bstate = 1;
			return 0;
		}
	}
	else if (n == 0)
	{
		return -2;
	}

	if ((size_t)n < size)
		iob->bstate = 1;

	return n;
}


/* 0	Would block
   -1	Error
   -2	End of file
 */
static inline ssize_t _do_write(iobuf_t *iob, const void *ptr, size_t size)
{
	ssize_t n;

	if (!iob->xio->write)
		NOT_SUPPORT(iob);
	if (iob->benable && iob->bstate)
		return 0;
again:
	n = iob->xio->write(iob->cookie, ptr, size);
	if (n < 0)
	{
		iob->err = errno;
		switch (iob->err)
		{
		case EINTR: 
			goto again;

		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
			iob->bstate = 1;
			return 0;

		case EPIPE:
			return -2;
		}
	}

	if ((size_t)n < size)
		iob->bstate = 1;

	return n;
}

static ssize_t _flush_internal(iobuf_t *iob)
{
	if (iob->wr && iob->len)
	{
		ssize_t rc;
		if (iob->cur + iob->len > iob->size)
		{
			ssize_t k = iob->size - iob->cur;
			if (k > 0)
			{
				rc = _do_write(iob, iob->buf + iob->cur, k);
				if (rc < k)
				{
					if (rc > 0)
					{
						iob->len -= rc;
						iob->cur += rc;
					}
					return rc;
				}
			}
			iob->len -= k;
			iob->cur = 0;
		}
		
		rc = _do_write(iob, iob->buf + iob->cur, iob->len);
		if (rc > 0)
		{
			iob->len -= rc;
			if (iob->len == 0)
				iob->cur = 0;
			else
				iob->cur += rc;
		}
		return rc;
	}
	return 0;
}


void iobuf_init(iobuf_t *iob, const xio_t *xio, void *cookie, unsigned char *buf, size_t size)
{
	assert(size <= INT_MAX);
	memset(iob, 0, sizeof(*iob));
	iob->xio = xio;
	iob->cookie = cookie;
	iob->buf = (unsigned char *)buf;
	iob->size = size;
}

int iobuf_finish(iobuf_t *iob)
{
	int rc = 0;

	if (iob->wr && iob->len)
	{
		ssize_t n = _flush_internal(iob);
		if (n <= 0 || iob->len)
			rc = -1;
	}

	if (iob->allocated)
		free(iob->buf);

	return rc;
}

ssize_t iobuf_flush(iobuf_t *iob)
{
	if (iob->wr && iob->len)
	{
		int len0 = iob->len;
		ssize_t rc = _flush_internal(iob);
		if (rc < 0)
			return rc;

		return len0 - iob->len;
	}
	return 0;
}


ssize_t iobuf_read(iobuf_t *iob, void *ptr, size_t n)
{
	CHECK_READ(iob);

	if (!iob->size)
	{
		return _do_read(iob, ptr, n);
	}

	if ((size_t)iob->len < n)
	{
		ssize_t k;
		ssize_t m = iob->len;
		ssize_t need = n - m;
		if (iob->len > 0)
		{
			memcpy(ptr, &iob->buf[iob->cur], iob->len);
			iob->len = 0;
		}
		iob->cur = 0;

		if (need <= 4096 && need <= iob->size / 2)
		{
			k = _do_read(iob, iob->buf, iob->size);
			if (k < 0)
				return (k == -1 || m == 0) ? k : m;
			else if (k == 0)
				return m;
		
			if (k <= need)
			{
				memcpy((char *)ptr + m, iob->buf, k);
				return m + k;
			}
			
			memcpy((char *)ptr + m, iob->buf, need);
			iob->cur = need;
			iob->len = k - need;
			if (!iob->len)
				iob->cur = 0;
			return n;
		}
		else
		{
			k = _do_read(iob, (char *)ptr + m, need);
			if (k < 0)
				return (k == -1 || m == 0) ? k : m;

			return m + k;
		}
	}
	else
	{
		if (n == 1)
			*(char *)ptr = iob->buf[iob->cur];
		else
			memcpy(ptr, &iob->buf[iob->cur], n);
		iob->cur += n;
		iob->len -= n;
		if (!iob->len)
			iob->cur = 0;
		return n;
	}
	return 0;
}

ssize_t iobuf_getdelim(iobuf_t *iob, char **p_result, char delim, bool readit)
{
	ssize_t n, k;
	unsigned char *p;

	CHECK_READ(iob);

	if (!iob->size)
	{
		OUT_OF_RANGE(iob);
	}

	if (iob->len > 0)
	{
		p = (unsigned char *)memchr(&iob->buf[iob->cur], (unsigned char)delim, iob->len);
		if (p)
		{
			++p;
			if (p_result)
				*p_result = (char *)&iob->buf[iob->cur];
			n = p - &iob->buf[iob->cur];
			iob->cur += n;
			iob->len -= n;
			if (!iob->len)
				iob->cur = 0;
			return n;
		}
		
		if (iob->len >= iob->size)
			OUT_OF_RANGE(iob);
	}

	if (!readit)
		return 0;

	if (iob->cur > 0)
	{
		if (iob->len > 0)
			memmove(iob->buf, &iob->buf[iob->cur], iob->len);
		iob->cur = 0;
	}

	k = _do_read(iob, &iob->buf[iob->len], iob->size - iob->len);
	if (k == 0)
		return 0;

	if (k < 0)
	{
		if (k == -1)
			return -1;
		if (p_result)
			*p_result = (char *)&iob->buf[iob->cur];
		n = iob->len;
		iob->cur = 0;
		iob->len = 0;
		return n > 0 ? n : k;
	}

	p = (unsigned char *)memchr(&iob->buf[iob->len], (unsigned char)delim, k);
	iob->len += k;
	if (p)
	{
		++p;
		if (p_result)
			*p_result = (char *)&iob->buf[iob->cur];
		n = p - &iob->buf[iob->cur];
		iob->cur += n;
		iob->len -= n;
		if (!iob->len)
			iob->cur = 0;
		return n;
	}

	if (iob->len >= iob->size)
		OUT_OF_RANGE(iob);
	return 0;
}

inline ssize_t iobuf_getdelim_xstr(iobuf_t *iob, xstr_t *xs, char delim, bool readit)
{
	char *buf;
	ssize_t rc = iobuf_getdelim(iob, &buf, delim, readit);
	if (xs && rc >= 0)
	{
		xstr_init(xs, (unsigned char *)buf, rc);
	}
	return rc;
}

ssize_t iobuf_getline(iobuf_t *iob, char **p_line)
{
	return iobuf_getdelim(iob, p_line, '\n', true);
}

ssize_t iobuf_getline_xstr(iobuf_t *iob, xstr_t *line)
{
	return iobuf_getdelim_xstr(iob, line, '\n', true);
}

ssize_t iobuf_peek(iobuf_t *iob, size_t n, char **p_result)
{
	ssize_t k;

	CHECK_READ(iob);

	if (n > (size_t)iob->size)
		INVALID_ARGUMENT(iob);

	if ((size_t)iob->len < n)
	{
		if (iob->len > 0 && iob->cur > 0)
			memmove(iob->buf, &iob->buf[iob->cur], iob->len);
		iob->cur = 0;
		k = _do_read(iob, &iob->buf[iob->len], iob->size - iob->len);
		if (k < 0)
			return k;

		iob->len += k;
	}

	if (p_result)
		*p_result = (char *)&iob->buf[iob->cur];

	return (size_t)iob->len < n ? iob->len : n;
}

static ssize_t _write_buffer(iobuf_t *iob, const void *ptr, size_t n)
{
	ssize_t rc;
	ssize_t written;
	ssize_t m, pos;
	ssize_t room;

	if ((size_t)iob->len + n <= (size_t)iob->size)
	{
		pos = iob->cur + iob->len;
		if (pos >= iob->size)
		{
			pos -= iob->size;
			if (n == 1)
				iob->buf[pos] = *(char *)ptr;
			else
				memcpy(&iob->buf[pos], ptr, n);
		}
		else
		{
			m = iob->size - pos;
			if ((size_t)m < n)
			{
				memcpy(&iob->buf[pos], ptr, m);
				memcpy(&iob->buf[0], (char *)ptr + m, n - m);
			}
			else if (n == 1)
				iob->buf[pos] = *(char *)ptr;
			else
				memcpy(&iob->buf[pos], ptr, n);
		}
		iob->len += n;
		return n;
	}

	if (iob->len)
	{
		rc = _flush_internal(iob);
		if (rc < 0)
			return rc;
	}

	written = 0;
	if (!iob->len)
	{
		rc = _do_write(iob, ptr, n);
		if (rc < 0)
			return rc;

		written = rc;
		ptr = (char *)ptr + written;
		n -= written;
	}

	if (n > 0 && iob->len < iob->size)
	{
		pos = iob->cur + iob->len;
		if (pos >= iob->size)
		{
			pos -= iob->size;
			room = iob->size - iob->len;
			m = n < (size_t)room ? n :room;
			memcpy(&iob->buf[pos], ptr, m);
			iob->len += m;
			written += m;
		}
		else
		{
			m = iob->size - pos;
			if ((size_t)m < n)
			{
				memcpy(iob->buf + pos, ptr, m);
				iob->len += m;
				ptr = (char *)ptr + m;
				n -= m;
				written += m;
				
				room = iob->size - iob->len;
				m = n < (size_t)room ? n : room;
				memcpy(iob->buf, ptr, m);
				iob->len += m;
				written += m;
			}
			else
			{
				memcpy(iob->buf + pos, ptr, n);
				iob->len += n;
				written += n;
			}
		}
	}

	return written;
}

ssize_t iobuf_write(iobuf_t *iob, const void *ptr, size_t n)
{
	CHECK_WRITE(iob);

	if (!iob->size)
	{
		return _do_write(iob, ptr, n);
	}

	return _write_buffer(iob, ptr, n);
}

ssize_t iobuf_puts(iobuf_t *iob, const char *str)
{
	ssize_t allow;
	ssize_t n;

	CHECK_WRITE(iob);

	if (!iob->size)
	{
		return _do_write(iob, str, strlen(str));
	}

	allow = iob->size - iob->cur;
	if (iob->len < allow)
	{
		ssize_t saved_len = iob->len;
		unsigned char *head = iob->buf + iob->cur;
		while (iob->len < allow && *str)
			head[iob->len++] = *str++;
		n = iob->len - saved_len;
	}
	else
		n = 0;

	if (*str)
	{
		ssize_t rc = _write_buffer(iob, str, strlen(str));
		if (rc < 0)
			return rc;
		n += rc;
	}

	return n;
}

ssize_t iobuf_pad(iobuf_t *iob, char c, size_t n)
{
	ssize_t k, m;

	CHECK_WRITE(iob);

	m = 0;
	if (!iob->size)
	{
		while (n > 0)
		{
			ssize_t rc;
			char buf[1024];

			k = n < 1024 ? n : 1024;
			memset(buf, c, k);
			rc = _do_write(iob, buf, k);
			if (rc < 0)
				return rc;
			m += rc;
			n -= rc;
			if (rc < k)
				break;
		}
	}
	else
	{
		unsigned char *p;
		unsigned char *end = iob->buf + iob->size;

		k = iob->size - iob->len;
		if ((size_t)k > n)
			k = n;

		if (k > 0)
		{
			p = iob->buf + iob->cur + iob->len;
			if (p < end)
			{
				size_t x = end - p;
				if (x > (size_t)k)
					x = k;
				memset(p, c, x);
				if (x < (size_t)k)
					memset(iob->buf, c, k - x);
			}
			else
			{
				p -= iob->size;
				memset(p, c, k);
			}
			iob->len += k;
			m += k;
			n -= k;
		}

		while  (n > 0)
		{
			ssize_t rc = _flush_internal(iob);
			if (rc < 0)
				return rc;

			k = iob->size - iob->len;
			if (k > 0)
			{
				p = iob->buf + iob->cur + iob->len;
				if ((size_t)k > n)
					k = n;

				if (p < end)
				{
					size_t x = end - p;
					if (x > (size_t)k)
						x = k;
					memset(p, c, x);
					if (x < (size_t)k)
						memset(iob->buf, c, k - x);
				}
				else
				{
					p -= iob->size;
					memset(p, c, k);
				}
				iob->len += k;
				m += k;
				n -= k;
			}

			if (k < iob->size)
				break;
		}
	}

	return m;
}

ssize_t iobuf_putc(iobuf_t *iob, char c)
{
	CHECK_WRITE(iob);

	if (!iob->size)
	{
		return _do_write(iob, &c, 1);
	}

	if (iob->len < iob->size)
	{
		size_t pos = iob->cur + iob->len;
		if (pos >= (size_t)iob->size)
			pos -= iob->size;
		iob->buf[pos] = c;
		iob->len++;
		return 1;
	}

	return _write_buffer(iob, &c, 1);
}

ssize_t iobuf_vprintf(iobuf_t *iob, const char *fmt, va_list ap)
{
	xio_write_function writer = (xio_write_function)(!iob->size ? _do_write : _write_buffer);
	char buf[256];

	CHECK_WRITE(iob);
	return vxformat(NULL, writer, iob, buf, sizeof(buf), fmt, ap);
}

ssize_t iobuf_printf(iobuf_t *iob, const char *fmt, ...)
{
	va_list ap;
	ssize_t r;

	va_start(ap, fmt);
	r = iobuf_vprintf(iob, fmt, ap);
	va_end(ap);
	return r;
}


ssize_t iobuf_reserve(iobuf_t *iob, size_t n, char **p_result)
{
	ssize_t rc;
	ssize_t m, pos, room;

	CHECK_WRITE(iob);

	if (n > (size_t)iob->size)
		INVALID_ARGUMENT(iob);

	pos = iob->cur + iob->len;
	if (pos >= iob->size)
	{
		pos -= iob->size;
		room = iob->size - iob->len;
	}
	else
	{
		room = iob->size - pos;
	}

	if ((size_t)room >= n)
	{
		if (p_result)
			*p_result = (char *)iob->buf + pos;
		return n;
	}

	
	rc = _flush_internal(iob);
	if (rc < 0)
		return rc;

	pos = iob->cur + iob->len;
	if (pos >= iob->size)
	{
		pos -= iob->size;
		room = iob->size - iob->len;
		m = n < (size_t)room ? n : room;
		if (p_result)
			*p_result = (char *)iob->buf + pos;
		return m;
	}

	room = iob->size - pos;
	if ((size_t)room >= n)
	{
		if (p_result)
			*p_result = (char *)iob->buf + pos;
		return n;
	}

	if (iob->len)
	{
		int newcur = iob->size - iob->len;
		memmove(iob->buf + newcur, iob->buf + iob->cur, iob->len);
		iob->cur = newcur;
	}
	else
		iob->cur = 0;

	if (p_result)
		*p_result = (char *)iob->buf;

	return room;
}


ssize_t iobuf_skip(iobuf_t *iob, ssize_t n)
{
	CHECK_ALLOC(iob);

	if (iob->wr)
	{
		ssize_t pos, room;

		if (n <= 0)
		{
			if (iob->len + n < 0)
				INVALID_ARGUMENT(iob);
		}
		else
		{
			pos = iob->cur + iob->len;
			room = (pos >= iob->size) ? iob->size - iob->len : iob->size - pos;

			if (n > room)
				INVALID_ARGUMENT(iob);
		}

		iob->len += n;
		return n;
	}
	else
	{
		ssize_t m;

		if (n <= 0)
		{
			if (iob->cur + n < 0)
				INVALID_ARGUMENT(iob);

			iob->cur += n;
			iob->len -= n;
			return n;
		}
		else
		{
			if (n < iob->len)
			{
				iob->len -= n;
				iob->cur += n;
				if (iob->cur > iob->size)
					iob->cur -= iob->size;
				return n;
			}

			m = iob->len;
			n -= m;
			iob->len = 0;
			iob->cur = 0;
			while (n > 0)
			{
				ssize_t k = _do_read(iob, iob->buf, iob->size);
				if (k < 0)
					return k;
				else if (k == 0)
					return m;

				if (k <= n)
				{
					m += k;
					n -= k;
				}
				else
				{
					iob->cur = n;
					iob->len = k - n;
					m += n;
					n = 0;
				}
			}
		}
		return m;
	}

	return -1;
}

int iobuf_seek(iobuf_t *iob, int64_t offset, int whence)
{
	ssize_t rc;

	if (iob->xio->seek)
	{
		if (iob->wr && iob->len)
		{
			rc = _flush_internal(iob);
			if (rc <= 0 || iob->len)
				return -1;
		}
		rc = iob->xio->seek(iob->cookie, &offset, whence);
		if (rc < 0)
			iob->err = errno;
		iob->len = 0;
		iob->cur = 0;
		return rc;
	}

	iob->err = ENOTSUP;
	return -1;
}

int64_t iobuf_tell(iobuf_t *iob)
{
	if (iob->xio->seek)
	{
		int rc;
		int64_t offset = 0;

		rc = iob->xio->seek(iob->cookie, &offset, SEEK_CUR);
		if (rc < 0)
		{
			iob->err = errno;
			return -1;
		}

		if (iob->len)
		{
			if (iob->wr)
				offset += iob->len;
			else
				offset -= iob->len;
		}

		return offset;
	}

	iob->err = ENOTSUP;
	return -1;
}


#ifdef TEST_IOBUF

static ssize_t _random_write(void *cookie, const void *data, size_t size)
{
	int x = 1 + random() % size;
	fwrite(data, x, 1, stdout);
	return x;
}

static xio_t xio = { NULL, _random_write, NULL, NULL };

#define BSIZE	32

int main()
{
	char chartab[] = "0123456789";
	char buf[1024];
	int i;
	int n = 0;
	iobuf_t iob = IOBUF_INIT(&xio, NULL, NULL, BSIZE);

	for (i = 0; i < 1024; ++i)
	{
		buf[i] = chartab[i % 10];
	}
		
	for (i = 0; i < 1024*1024; ++i)
	{
		int x = 1 + random() % (BSIZE * 2);
		int r = iobuf_write(&iob, &buf[n % 10], x);
		assert(r >= 0);
		n += r;
	}

	return 0;
}

#endif
