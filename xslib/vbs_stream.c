#include "vbs_stream.h"
#include <limits.h>
#include <endian.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: vbs_stream.c,v 1.5 2015/05/13 04:21:43 gremlin Exp $";
#endif

#ifndef SSIZE_MAX
#define SSIZE_MAX	(SIZE_MAX/2)
#endif

static ssize_t _fill_buffer(vbs_stream_unpacker_t *job)
{
	ssize_t n;
	ssize_t len = job->uk.end - job->uk.cur;
	if (job->uk.buf < job->uk.cur)
	{
		if  (len > 0)
		{
			memmove(job->uk.buf, job->uk.cur, len);
		}
		job->uk.cur = job->uk.buf;
		job->uk.end = job->uk.cur + len;
	}

	n = job->read(job->cookie, job->uk.end, sizeof(job->_buffer) - len);
	if (n > 0)
		job->uk.end += n;

	return n;
}

inline vbs_type_t vbs_stream_unpack_type(vbs_stream_unpacker_t *job, intmax_t *p_number)
{
	int rc = vbs_unpack_type(&job->uk, p_number);
	if (rc == VBS_ERR_INCOMPLETE)
	{
		ssize_t n = _fill_buffer(job);
		if (n <= 0)
			return rc;
		rc = vbs_unpack_type(&job->uk, p_number);
	}
	return rc;
}

bool vbs_stream_unpack_if_tail(vbs_stream_unpacker_t *job)
{
	if (job->uk.cur >= job->uk.end)
	{
		ssize_t n = _fill_buffer(job);
		if (n <= 0)
			return false;
	}
	return vbs_unpack_if_tail(&job->uk);
}

ssize_t vbs_stream_unpack_read(vbs_stream_unpacker_t *job, void *data, size_t size)
{
	ssize_t n;
	ssize_t len = job->uk.end - job->uk.cur;

	if (len >= (ssize_t)size)
	{
		memcpy(data, job->uk.cur, size);
		job->uk.cur += size;
		return size;
	}

	if (len > 0)
	{
		memcpy(data, job->uk.cur, len);
		job->uk.cur = job->uk.buf;
		job->uk.end = job->uk.cur;
	}

	n = job->read(job->cookie, data + len, size - len);
	if (n < 0)
		return n;
	n += len;
	return n;
}

ssize_t vbs_stream_unpack_skip(vbs_stream_unpacker_t *job, size_t size)
{
	ssize_t left;
	ssize_t len = job->uk.end - job->uk.cur;

	if (len >= (ssize_t)size)
	{
		job->uk.cur += size;
		return size;
	}

	job->uk.cur = job->uk.buf;
	job->uk.end = job->uk.cur;

	left = size - len;
	while (left > 0)
	{
		char buf[4096];
		ssize_t num = left < sizeof(buf) ? left : sizeof(buf);
		ssize_t rc = job->read(job->cookie, buf, num);
		if (rc < 0)
			return rc;

		left -= rc;
		if (rc < num)
			return size - left;
	}

	return size;
}

int vbs_stream_unpack_integer(vbs_stream_unpacker_t *job, intmax_t *p_value)
{
	if (VBS_INTEGER != vbs_stream_unpack_type(job, p_value))
		return -1;
	return 0;
}

int vbs_stream_unpack_floating(vbs_stream_unpacker_t *job, double *p_value)
{
	intmax_t significant;
	intmax_t expo;

	if (VBS_FLOATING != vbs_stream_unpack_type(job, &significant))
		return -1;

	if (vbs_stream_unpack_integer(job, &expo) < 0)
		return -1;

	if (vbs_make_double_value(p_value, significant, expo) < 0)
		return -1;
	return 0;
}

int vbs_stream_unpack_decimal64(vbs_stream_unpacker_t *job, decimal64_t *p_value)
{
	intmax_t significant;
	intmax_t expo;

	if (VBS_DECIMAL != vbs_stream_unpack_type(job, &significant))
		return -1;

	if (vbs_stream_unpack_integer(job, &expo) < 0)
		return -1;

	if (vbs_make_decimal64_value(p_value, significant, expo) < 0)
		return -1;
	return 0;
}

int vbs_stream_unpack_bool(vbs_stream_unpacker_t *job, bool *p_value)
{
	intmax_t num;
	if (VBS_BOOL != vbs_stream_unpack_type(job, &num))
		return -1;
	*p_value = num;
	return 0;
}

int vbs_stream_unpack_null(vbs_stream_unpacker_t *job)
{
	intmax_t num;
	if (VBS_NULL != vbs_stream_unpack_type(job, &num))
		return -1;
	return 0;
}

static inline int _unpack_simple_head(vbs_stream_unpacker_t *job, vbs_type_t type, ssize_t *p_len)
{
	intmax_t num;
	if (type != vbs_stream_unpack_type(job, &num) || num < 0 || num > SSIZE_MAX)
		return -1;

	*p_len = num;
	return 0;
}

int vbs_stream_unpack_head_of_string(vbs_stream_unpacker_t *job, ssize_t *p_len)
{
	return _unpack_simple_head(job, VBS_STRING, p_len);
}

int vbs_stream_unpack_head_of_blob(vbs_stream_unpacker_t *job, ssize_t *p_len)
{
	return _unpack_simple_head(job, VBS_BLOB, p_len);
}

static inline int _unpack_composite_head(vbs_stream_unpacker_t *job, vbs_type_t type, ssize_t *p_len)
{
	intmax_t num;
	if (type != vbs_stream_unpack_type(job, &num) || num < 0 || num > SSIZE_MAX)
		return -1;

	*p_len = num;
	job->uk.depth++;
	if (job->uk.max_depth && job->uk.depth > job->uk.max_depth)
		return -1;

	return 0;
}

int vbs_stream_unpack_head_of_list_with_length(vbs_stream_unpacker_t *job, ssize_t *p_len)
{
	return _unpack_composite_head(job, VBS_LIST, p_len);
}

int vbs_stream_unpack_head_of_dict_with_length(vbs_stream_unpacker_t *job, ssize_t *p_len)
{
	return _unpack_composite_head(job, VBS_DICT, p_len);
}

int vbs_stream_unpack_head_of_list(vbs_stream_unpacker_t *job)
{
	ssize_t len;
	return _unpack_composite_head(job, VBS_LIST, &len);
}

int vbs_stream_unpack_head_of_dict(vbs_stream_unpacker_t *job)
{
	ssize_t len;
	return _unpack_composite_head(job, VBS_DICT, &len);
}

int vbs_stream_unpack_tail(vbs_stream_unpacker_t *job)
{
	intmax_t num;

	if (job->uk.depth <= 0)
		return -1;

	if (VBS_TAIL != vbs_stream_unpack_type(job, &num))
		return -1;

	job->uk.depth--;
	return 0;
}

/* The content of list or dict is decoded too. Need to call vbs_release_data()
   with the same arguments of xm and xm_cookie, if the value is list or dict.
 */
int vbs_stream_unpack_data(vbs_stream_unpacker_t *job, vbs_data_t *data, const xmem_t *xm, void *xm_cookie);
int vbs_stream_unpack_list(vbs_stream_unpacker_t *job, vbs_list_t *list, const xmem_t *xm, void *xm_cookie);
int vbs_stream_unpack_dict(vbs_stream_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie);


/* Call following functions after vbs_unpack_primitive() */
int vbs_stream_unpack_body_of_list(vbs_stream_unpacker_t *job, ssize_t len, vbs_list_t *list, const xmem_t *xm, void *xm_cookie);
int vbs_stream_unpack_body_of_dict(vbs_stream_unpacker_t *job, ssize_t len, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie);



