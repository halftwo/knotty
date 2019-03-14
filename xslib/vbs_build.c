#include "vbs_build.h"
#include "xbuf.h"
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: vbs_build.c,v 1.6 2015/05/28 21:48:21 gremlin Exp $";
#endif

#define BOX_STACK_DEPTH		32

typedef struct
{
	const char *fmt;
	va_list ap;
} context_t;

enum box_state {
	UNKNOWN_BOX = 0,
	LIST_BOX = 1,
	DICT_BOX = 2,
};

enum parse_state
{
	PS_KEY,
	PS_EQ,
	PS_VALUE,
	PS_SEP,
};

/*
 * We'll check the format string with our best.
 * Not all errors can be checked.
 */
static int _do_format(vbs_packer_t *job, context_t *ctx, enum box_state box)
{
	union {
		intmax_t i;
		bool t;
                double f;
		decimal64_t dec;
                const char *s;
		const void *blob;
		const xstr_t *xs;
		const vbs_list_t *l;
		const vbs_dict_t *m;
		const vbs_data_t *x;
		vbs_build_callback_function cb;
	} val;
	void *cb_ctx;
	ssize_t len;
	int packer_depth_before_cb;
	int box_depth = 0;
	unsigned char box_stack[BOX_STACK_DEPTH];
	unsigned char ch;
	enum parse_state ps = (box == DICT_BOX) ? PS_KEY : PS_VALUE;
	int maybe_list = 0;

	while ((ch = *ctx->fmt++) != 0)
	{
		switch (ch)
		{
		case 'i':
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			++ps;
			val.i = va_arg(ctx->ap, int);
			vbs_pack_integer(job, val.i);
			break;

		case 'I':
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			++ps;
			val.i = va_arg(ctx->ap, intmax_t);
			vbs_pack_integer(job, val.i);
			break;

		case 't':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			val.t = va_arg(ctx->ap, int);	/* bool is promoted to int */
			vbs_pack_bool(job, val.t);
			break;

		case 'f':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			val.f = va_arg(ctx->ap, double);
			vbs_pack_floating(job, val.f);
			break;

		case 'd':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			val.dec = va_arg(ctx->ap, decimal64_t);
			vbs_pack_decimal64(job, val.dec);
			break;

		case 'S':
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			++ps;
			val.xs = va_arg(ctx->ap, const xstr_t *);
			vbs_pack_xstr(job, val.xs);
			break;

		case 'B':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			val.xs = va_arg(ctx->ap, const xstr_t *);
			vbs_pack_blob(job, val.xs->data, val.xs->len);
			break;

		case 'R':
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			ps = PS_SEP;
			val.xs = va_arg(ctx->ap, const xstr_t *);
			vbs_pack_raw(job, val.xs->data, val.xs->len);
			break;

		case 's':
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			++ps;
			val.s = va_arg(ctx->ap, const char *);
			if (*ctx->fmt == '*' || *ctx->fmt == '#')
			{
				len = (*ctx->fmt == '*') ? va_arg(ctx->ap, int) : va_arg(ctx->ap, ssize_t);
				++ctx->fmt;
				vbs_pack_lstr(job, val.s, len);
			}
			else
			{
				vbs_pack_cstr(job, val.s);
			}
			break;

		case 'b':
			if (*ctx->fmt != '*' && *ctx->fmt != '#')
				goto error;
			if (ps != PS_VALUE)
				goto error;
			++ps;
			val.blob = va_arg(ctx->ap, const void *);
			len = (*ctx->fmt == '*') ? va_arg(ctx->ap, int) : va_arg(ctx->ap, ssize_t);
			++ctx->fmt;
			vbs_pack_blob(job, val.blob, len);
			break;

		case 'r':
			if (*ctx->fmt != '*' && *ctx->fmt != '#')
				goto error;
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			ps = PS_SEP;
			val.blob = va_arg(ctx->ap, const void *);
			len = (*ctx->fmt == '*') ? va_arg(ctx->ap, int) : va_arg(ctx->ap, ssize_t);
			++ctx->fmt;
			vbs_pack_raw(job, val.blob, len);
			--maybe_list;
			break;

		case 'l':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			val.l = va_arg(ctx->ap, const vbs_list_t *);
			vbs_pack_list(job, val.l);
			break;
			
		case 'm':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			val.m = va_arg(ctx->ap, const vbs_dict_t *);
			vbs_pack_dict(job, val.m);
			break;

		case 'x':
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			++ps;
			val.x = va_arg(ctx->ap, const vbs_data_t *);
			if (ps == PS_KEY && val.x->kind != VBS_INTEGER && val.x->kind != VBS_STRING)
				goto error;
			vbs_pack_data(job, val.x);
			break;

		case '@':
			if (ps != PS_KEY && ps != PS_VALUE)
				goto error;
			ps = PS_SEP;
			val.cb = va_arg(ctx->ap, vbs_build_callback_function);
			cb_ctx = va_arg(ctx->ap, void *);
			packer_depth_before_cb = job->depth;
			if (val.cb(job, cb_ctx) < 0)
				goto error;
			if (job->depth != packer_depth_before_cb)
				goto error;
			--maybe_list;
			break;

		case 'n':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			vbs_pack_null(job);
			break;

		case '{':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			if (vbs_pack_head_of_dict0(job) < 0)
				goto error;
			ps = PS_KEY;
			if (box_depth < BOX_STACK_DEPTH)
				box_stack[box_depth] = box;
			++box_depth;
			box = DICT_BOX;
			break;

		case '}':
			if (box == LIST_BOX || !(ps == PS_SEP || box == UNKNOWN_BOX 
				|| (box == DICT_BOX && ps == PS_KEY)))
			{
				goto error;
			}
			if (vbs_pack_tail(job) < 0)
				goto error;
			ps = PS_SEP;
			--box_depth;
			box = (box_depth >= 0 && box_depth < BOX_STACK_DEPTH) ? box_stack[box_depth] : UNKNOWN_BOX;
			if (box == UNKNOWN_BOX)
				maybe_list = 0;
			break;

		case '[':
			if (ps != PS_VALUE)
				goto error;
			++ps;
			if (vbs_pack_head_of_list0(job) < 0)
				goto error;
			ps = PS_VALUE;
			if (box_depth < BOX_STACK_DEPTH)
				box_stack[box_depth] = box;
			++box_depth;
			box = LIST_BOX;
			break;

		case ']':
			if (box == DICT_BOX)
				goto error;
			if (vbs_pack_tail(job) < 0)
				goto error;
			ps = PS_SEP;
			--box_depth;
			box = (box_depth >= 0 && box_depth < BOX_STACK_DEPTH) ? box_stack[box_depth] : UNKNOWN_BOX;
			if (box == UNKNOWN_BOX)
				maybe_list = 0;
			break;

		case '^':
			if (box == UNKNOWN_BOX)
			{
				if (ps != PS_SEP)
					goto error;
				box = DICT_BOX;
				ps = PS_EQ;
			}
			else if (ps != PS_EQ)
				goto error;
			++ps;
			break;

		case ';':
			if (box == UNKNOWN_BOX)
			{
				if (ps == PS_SEP)
				{
					++maybe_list;
					if (maybe_list >= 2)
						box = LIST_BOX;
				}
			}
			else if (ps != PS_SEP)
				goto error;
			ps = (box == DICT_BOX) ? PS_KEY : PS_VALUE;
			break;

		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\v':
		case '\f':
			/* do nothing */
			break;

		default:
			goto error;
		}
	}

	--ctx->fmt;
	return 0;
error:
	job->error = -1;
	return -1;
}


inline int vbs_vbuild_format(vbs_packer_t *job, const char *format, va_list ap)
{
	context_t ctx;

	ctx.fmt = format;
	va_copy(ctx.ap, ap);

	if (_do_format(job, &ctx, UNKNOWN_BOX) < 0)
		return -1;

	return 0;
}


int vbs_build_format(vbs_packer_t *job, const char *format, ...)
{
	int rc;
	va_list ap;

	va_start(ap, format);
	rc = vbs_vbuild_format(job, format, ap);
	va_end(ap);
	return rc;
}

inline int vbs_vbuild_write(xio_write_function xio_write, void *xio_ctx, const char *format, va_list ap)
{
	vbs_packer_t pk = VBS_PACKER_INIT(xio_write, xio_ctx, -1);
	int rc = vbs_vbuild_format(&pk, format, ap);
	if (rc < 0 || pk.error || pk.depth)
		return -1;
	return 0;
}

int vbs_build_write(xio_write_function xio_write, void *xio_ctx, const char *fmt, ...)
{
	int rc;
	va_list ap;

	va_start(ap, fmt);
	rc = vbs_vbuild_write(xio_write, xio_ctx, fmt, ap);
	va_end(ap);
	return rc;
}

static ssize_t _write(void *cookie, const void *data, size_t size)
{
	xbuf_t *xb = (xbuf_t *)cookie;
	ssize_t avail = xb->capacity - xb->len;
	if (avail > 0)
	{
		ssize_t n = (size < avail) ? size : avail;

		if (n > 1)
			memcpy(xb->data + xb->len, data, n);
		else if (n == 1)
			xb->data[xb->len] = *(unsigned char *)data;
	}

	xb->len += size;
	return size;
}

inline ssize_t vbs_vbuild_buf(void *buf, size_t size, const char *format, va_list ap)
{
	xbuf_t xb = XBUF_INIT((unsigned char *)buf, size);
	vbs_packer_t pk = VBS_PACKER_INIT(_write, &xb, -1);
	int rc = vbs_vbuild_format(&pk, format, ap);
	if (rc < 0 || pk.error || pk.depth)
		return -1;
	return xb.len;
}

ssize_t vbs_build_buf(void *buf, size_t size, const char *fmt, ...)
{
	ssize_t rc;
	va_list ap;
	va_start(ap, fmt);
	rc = vbs_vbuild_buf(buf, size, fmt, ap);
	va_end(ap);
	return rc;
}

ssize_t vbs_vbuild_alloc(unsigned char **pbuf, const char *fmt, va_list ap)
{
	unsigned char buf[4096];
	ssize_t rc;
	va_list ap2;

	va_copy(ap2, ap);
	rc = vbs_vbuild_buf(buf, sizeof(buf), fmt, ap);
	va_end(ap2);
	if (rc <= 0)
		return rc;

	*pbuf = (unsigned char *)malloc(rc);
	if (!*pbuf)
		return -1;

	if (rc <= sizeof(buf))
	{
		memcpy(*pbuf, buf, rc);
	}
	else
	{
		ssize_t n = vbs_vbuild_buf(*pbuf, rc, fmt, ap);
		assert(n == rc);
	}
	return rc;
}

ssize_t vbs_build_alloc(unsigned char **pbuf, const char *fmt, ...)
{
	ssize_t rc;
	va_list ap;
	va_start(ap, fmt);
	rc = vbs_vbuild_alloc(pbuf, fmt, ap);
	va_end(ap);
	return rc;
}


#ifdef TEST_VBS_BUILD

#include "vbs.h"

static int callback(vbs_packer_t *job, void *ctx)
{
	return vbs_build_format(job, "{s^i}", "fine", 333);
}

int main(int argc, char **argv)
{
	int rc;
	ssize_t len, len2;
	unsigned char buf[1024];
	vbs_unpacker_t uk;
	vbs_packer_t pk;
	xstr_t xs = XSTR_CONST("xstr_example");
	unsigned char *p = buf;

	vbs_packer_init(&pk, pptr_xio.write, &p, -1);
	vbs_build_format(&pk, "{s^i;s*^I;S^", "hello", 1, "world", 4, (intmax_t)2, &xs);
	vbs_build_format(&pk, "{i^[n;t;", 3, true);
	vbs_build_format(&pk, "s;]};s^@}", "good", "we", callback, NULL);
	len = p - buf;

	len2 = vbs_build_buf(buf, sizeof(buf), "{s^i;s*^I;S^{i^[n;t;s;];};s^@}",
			"hello", 1,
			"world", 4, (intmax_t)2,
			&xs, 3, true, "good",
			"we", callback, NULL);

	assert(len == len2);

	vbs_unpacker_init(&uk, buf, len, -1);
	rc = vbs_unpack_raw(&uk, NULL, &len2);
	assert(rc == VBS_DICT && len == len2);

	vbs_unpacker_init(&uk, buf, len, -1);
	iobuf_t ob = IOBUF_INIT(&stdio_xio, stdout, NULL, 0);
	fprintf(stdout, "-------------- BEGIN\n");
	rc = vbs_unpack_print_all(&uk, &ob);
	assert(rc == 0);
	fprintf(stdout, "\n-------------- END\n");
	return 0;
}

#endif
