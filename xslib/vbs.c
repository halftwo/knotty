#include "vbs.h"
#include "xformat.h"
#include "decimal64.h"
#include <limits.h>
#include <endian.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: vbs.c,v 1.94 2015/06/16 03:09:01 gremlin Exp $";
#endif

#define TMPBUF_SIZE	24

#ifndef SSIZE_MAX
#define SSIZE_MAX	(SIZE_MAX / 2)
#endif



int vbs_pack_raw_rope(vbs_packer_t *job, const rope_t *rope)
{
	rope_block_t *block = NULL;
	unsigned char *buf;
	ssize_t len;

	while (rope_next_block(rope, &block, &buf, &len))
	{
		if (len > 0)
		{
			if (job->write(job->cookie, buf, len) != (ssize_t)len)
				return -1;
		}
	}
	return 0;
}

int vbs_pack_string_vprintf(vbs_packer_t *job, xfmt_callback_function xfmt_cb, const char *fmt, va_list ap)
{
	va_list ap2;
	char buf[256];
	ssize_t n;

	va_copy(ap2, ap);
	n = vxformat(xfmt_cb, NULL, NULL, buf, sizeof(buf), fmt, ap2);
	va_end(ap2);
	if (n < 0)
		return -1;

	if (vbs_pack_head_of_string(job, n) < 0)
		return -1;

	if (n < (ssize_t)sizeof(buf))
	{
		if (job->write(job->cookie, buf, n) != n)
			return -1;
	}
	else
	{
		ssize_t r = vxformat(xfmt_cb, job->write, job->cookie, buf, sizeof(buf), fmt, ap);
		if (r != n)
		{
			job->error = -1;
			return -1;
		}
	}
	return 0;
}

int vbs_pack_string_printf(vbs_packer_t *job, xfmt_callback_function xfmt_cb, const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vbs_pack_string_vprintf(job, xfmt_cb, fmt, ap);
	va_end(ap);
	return r;
}

static int _copy_list(vbs_list_t *dst, const vbs_list_t *src, const xmem_t *xm, void *xm_cookie);
static int _copy_dict(vbs_dict_t *dst, const vbs_dict_t *src, const xmem_t *xm, void *xm_cookie);

static inline int _copy_data(vbs_data_t *dst, const vbs_data_t *src, const xmem_t *xm, void *xm_cookie)
{
	dst->descriptor = src->descriptor;
	if (src->type == VBS_STRING)
	{
		size_t len = src->d_xstr.len;
		char *buf = (char *)xm->alloc(xm_cookie, len);
		if (!buf)
			return -1;
		memcpy(buf, src->d_xstr.data, len);
		vbs_data_set_lstr(dst, buf, len, true);
	}
	else if (src->type == VBS_BLOB)
	{
		size_t len = src->d_blob.len;
		void *buf = xm->alloc(xm_cookie, len);
		if (!buf)
			return -1;
		memcpy(buf, src->d_blob.data, len);
		vbs_data_set_blob(dst, buf, len, true);
	}
	else if (src->type == VBS_LIST)
	{
		vbs_data_set_list(dst, (vbs_list_t *)xm->alloc(xm_cookie, sizeof(vbs_list_t)));
		return _copy_list(dst->d_list, src->d_list, xm, xm_cookie);
	}
	else if (src->type == VBS_DICT)
	{
		vbs_data_set_dict(dst, (vbs_dict_t *)xm->alloc(xm_cookie, sizeof(vbs_dict_t)));
		return _copy_dict(dst->d_dict, src->d_dict, xm, xm_cookie);
	}
	else
	{
		*dst = *src;
	}
	return 0;
}

static int _copy_list(vbs_list_t *dst, const vbs_list_t *src, const xmem_t *xm, void *xm_cookie)
{
	vbs_litem_t *se;

	vbs_list_init(dst, src->kind);
	for (se = src->first; se; se = se->next)
	{
		vbs_litem_t *de = (vbs_litem_t *)xm->alloc(xm_cookie, sizeof(*de));
		if (!de)
			return -1;
		if (_copy_data(&de->value, &se->value, xm, xm_cookie) < 0)
			return -1;
		vbs_list_push_back(dst, de);
	}
	return 0;
}

static int _copy_dict(vbs_dict_t *dst, const vbs_dict_t *src, const xmem_t *xm, void *xm_cookie)
{
	vbs_ditem_t *se;

	vbs_dict_init(dst, src->kind);
	for (se = src->first; se; se = se->next)
	{
		vbs_ditem_t *de = (vbs_ditem_t *)xm->alloc(xm_cookie, sizeof(*de));
		if (!de)
			return -1;
		if (_copy_data(&de->key, &se->key, xm, xm_cookie) < 0)
			return -1;
		if (_copy_data(&de->value, &se->value, xm, xm_cookie) < 0)
			return -1;
		vbs_dict_push_back(dst, de);
	}
	return 0;
}

int vbs_copy_list(vbs_list_t *dst, const vbs_list_t *src, const xmem_t *xm, void *xm_cookie)
{
	return _copy_list(dst, src, xm ? xm : &stdc_xmem, xm_cookie);
}

int vbs_copy_dict(vbs_dict_t *dst, const vbs_dict_t *src, const xmem_t *xm, void *xm_cookie)
{
	return _copy_dict(dst, src, xm ? xm : &stdc_xmem, xm_cookie);
}

int vbs_copy_data(vbs_data_t *dst, const vbs_data_t *src, const xmem_t *xm, void *xm_cookie)
{
	return _copy_data(dst, src, xm ? xm : &stdc_xmem, xm_cookie);
}



int vbs_print_list(const vbs_list_t *list, iobuf_t *ob)
{
	vbs_litem_t *ent;

	if (list->kind)
	{
		if (iobuf_printf(ob, "%d[", list->kind) < 0)
			return -1;
	}
	else
	{
		if (iobuf_putc(ob, '[') < 1)
			return -1;
	}

	for (ent = list->first; ent; ent = ent->next)
	{
		if (ent != list->first)
		{
			if (iobuf_puts(ob, "; ") < 2)
				return -1;
		}

		if (vbs_print_data(&ent->value, ob) < 0)
			return -1;
	}

	if (iobuf_putc(ob, ']') < 1)
		return -1;
	return 0;
}

int vbs_print_dict(const vbs_dict_t *dict, iobuf_t *ob)
{
	vbs_ditem_t *ent;

	if (dict->kind)
	{
		if (iobuf_printf(ob, "%d{", dict->kind) < 0)
			return -1;
	}
	else
	{
		if (iobuf_putc(ob, '{') < 1)
			return -1;
	}

	for (ent = dict->first; ent; ent = ent->next)
	{
		if (ent != dict->first)
		{
			if (iobuf_puts(ob, "; ") < 2)
				return -1;
		}

		if (vbs_print_data(&ent->key, ob) < 0)
			return -1;
		if (iobuf_putc(ob, '^') < 1)
			return -1;
		if (vbs_print_data(&ent->value, ob) < 0)
			return -1;
	}

	if (iobuf_putc(ob, '}') < 1)
		return -1;
	return 0;
}


/* 
 * cntrl_bset plus ^~`;[]{}
 */
const bset_t vbs_meta_bset =
{
	{
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x08000000, /* 0000 1000 0000 0000  0000 0000 0000 0000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x68000000, /* 0110 1000 0000 0000  0000 0000 0000 0000 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0xe8000001, /* 1110 1000 0000 0000  0000 0000 0000 0001 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x80000000, /* 1000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

/*
 * alpha_bset plus #$%&()*./:<=>?@\_
 */
static const bset_t token0_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xf40087fe, /* 1111 0100 0000 0000  1000 0111 0111 1000 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x97ffffff, /* 1001 0111 1111 1111  1111 1111 1111 1111 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x17fffffe, /* 0000 0111 1111 1111  1111 1111 1111 1110 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

/*
 * graph_bset except vbs_meta_bset and ,
 */
static const bset_t token1_bset = 
{
	{
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xf7ffeffe, /* 1111 0111 1111 1111  1110 1111 1111 1110 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x97ffffff, /* 1001 0111 1111 1111  1111 1111 1111 1111 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x17fffffe, /* 0001 0111 1111 1111  1111 1111 1111 1110 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

#define ISMETA(ch)	BSET_TEST(&vbs_meta_bset, (ch))
#define ISTOKEN0(ch)	BSET_TEST(&token0_bset, (ch))

int vbs_string_payload_default_print(iobuf_t *sink, const xstr_t *xs, bool is_blob)
{
	size_t pos = 0;
	xstr_t s = *xs;

	while (pos < s.len)
	{
		int ch = (unsigned char)s.data[pos];
		if (ISMETA(ch) || (is_blob && ch >= 0x80))
		{
			int b1 = ch >> 4;
			int b2 = ch & 0x0F;
			char esc[4];

			esc[0] = '`';
			esc[1] = b1 < 10 ? (b1 + '0') : (b1 - 10 + 'A');
			esc[2] = b2 < 10 ? (b2 + '0') : (b2 - 10 + 'A');

			if (pos)
			{
				if (iobuf_write(sink, s.data, pos) < pos)
					return -1;
			}

			if (iobuf_write(sink, esc, 3) < 3)
				return -1;

			++pos;
			s.data += pos;
			s.len -= pos;
			pos = 0;
		}
		else
		{
			++pos;
			if (pos >= 1024)
			{
				if (iobuf_write(sink, s.data, pos) < pos)
					return -1;
				s.data += pos;
				s.len -= pos;
				pos = 0;
			}
		}
	}

	if (s.len)
	{
		if (iobuf_write(sink, s.data, s.len) < s.len)
			return -1;
	}

	return 0;
}

static vbs_string_payload_print_function _print_payload = vbs_string_payload_default_print;

void vbs_set_string_payload_print_function(vbs_string_payload_print_function print)
{
	_print_payload = print ? print : vbs_string_payload_default_print;
}

static int _print_string(iobuf_t *sink, const xstr_t *xs, bool blob)
{
	bool special = false;

	if (xs->len == 0)
	{
		if (iobuf_write(sink, blob ? "~|~" : "~!~", 3) < 3)
			return -1;
		return 0;
	}

	if (blob || xs->len >= 100)
	{
		char buf[24];
		size_t len = xfmt_snprintf(NULL, buf, sizeof(buf), "~%zd%c", xs->len, blob ? '|' : '!');
		if (iobuf_write(sink, buf, len) < len)
			return -1;

		special = true;
	}
	else if (!ISTOKEN0(xs->data[0]) || xstr_find_not_in_bset(xs, 0, &token1_bset) >= 0)
	{
		if (iobuf_write(sink, "~!", 2) < 2)
			return -1;

		special = true;
	}

	if (_print_payload(sink, xs, blob) < 0)
		return -1;

	if (special)
	{
		if (iobuf_putc(sink, '~') < 1)
			return -1;
	}

	return 0;
}

inline int vbs_print_string(const xstr_t *xs, iobuf_t *ob)
{
	return _print_string(ob, xs, false);
}

inline int vbs_print_blob(const xstr_t *blob, iobuf_t *ob)
{
	return _print_string(ob, blob, true);
}

int vbs_print_decimal64(decimal64_t value, iobuf_t *ob)
{
	char buf[DECIMAL64_STRING_MAX];
	decimal64_to_cstr(value, buf);
	if (iobuf_printf(ob, "%sD", buf) < 0)
		return -1;
	return 0;
}

static int _print_hidden(const vbs_data_t *pv, iobuf_t *ob)
{
	int r = 0;
	switch (pv->type)
	{
	case VBS_INTEGER:
		r = iobuf_puts(ob, "~%i~");
		break;
	case VBS_STRING:
		r = iobuf_puts(ob, "~%s~");
		break;
	case VBS_BOOL:
		r = iobuf_puts(ob, "~%t~");
		break;
	case VBS_FLOATING:
		r = iobuf_puts(ob, "~%f~");
		break;
	case VBS_DECIMAL:
		r = iobuf_puts(ob, "~%d~");
		break;
	case VBS_BLOB:
		r = iobuf_puts(ob, "~%b~");
		break;
	case VBS_NULL:
		r = iobuf_puts(ob, "~%n~");
		break;
	case VBS_DICT:
		r = iobuf_puts(ob, "~{}~");
		break;
	case VBS_LIST:
		r = iobuf_puts(ob, "~[]~");
		break;
	default:
		r = -1;
	}
	return r;
}

int vbs_print_data(const vbs_data_t *pv, iobuf_t *ob)
{
	int r = 0;
	if ((pv->descriptor & VBS_SPECIAL_DESCRIPTOR) != 0)
	{
		r = _print_hidden(pv, ob);
	}
	else
	{
		if (pv->descriptor)
		{
			if (iobuf_printf(ob, "%d@", pv->descriptor) < 0)
				return -1;
		}

		switch (pv->type)
		{
		case VBS_INTEGER:
			r = iobuf_printf(ob, "%jd", pv->d_int);
			break;
		case VBS_STRING:
			r = vbs_print_string(&pv->d_xstr, ob);
			break;
		case VBS_BOOL:
			r = iobuf_puts(ob, pv->d_bool ? "~T" : "~F");
			break;
		case VBS_FLOATING:
			r = iobuf_printf(ob, "%#.17G", pv->d_floating);
			break;
		case VBS_DECIMAL:
			r = vbs_print_decimal64(pv->d_decimal64, ob);
			break;
		case VBS_BLOB:
			r = vbs_print_blob(&pv->d_blob, ob);
			break;
		case VBS_NULL:
			r = iobuf_puts(ob, "~N");
			break;
		case VBS_DICT:
			r = vbs_print_dict(pv->d_dict, ob);
			break;
		case VBS_LIST:
			r = vbs_print_list(pv->d_list, ob);
			break;
		default:
			r = -1;
		}
	}
	return r < 0 ? -1 : 0;
}

static int _do_unpack_print(vbs_unpacker_t *job, iobuf_t *ob, const vbs_data_t *pv, int kind);

static int _dump_to_tail(vbs_unpacker_t *job, bool dict, iobuf_t *ob, int kind)
{
	size_t n;
	int rc = -1;

	if (kind)
	{
		if (iobuf_printf(ob, "%d", kind) < 0)
			goto error;
	}

	if (iobuf_putc(ob, dict ? '{' : '[') != 1)
		goto error;

	for (n = 0; true; ++n)
	{
		vbs_data_t val;
		int kind;
		int r = vbs_unpack_primitive(job, &val, &kind);
		if (r < 0)
			goto error;

		if (val.type == VBS_TAIL)
		{
			if (dict && (n % 2))
				goto error;

			if (iobuf_putc(ob, dict ? '}' : ']') != 1)
				goto error;

			break;
		}

		if (n > 0)
		{
			if (dict && (n % 2))
			{
				if (iobuf_putc(ob, '^') != 1)
					goto error;
			}
			else
			{
				if (iobuf_puts(ob, "; ") != 2)
					goto error;
			}
		}

		r = _do_unpack_print(job, ob, &val, kind);
		if (r < 0)
			goto error;
	}
	rc = 0;
error:
	return rc;
}

static int _do_unpack_print(vbs_unpacker_t *job, iobuf_t *ob, const vbs_data_t *pv, int kind)
{
	if ((pv->descriptor & VBS_SPECIAL_DESCRIPTOR) != 0)
	{
		if (pv->type == VBS_DICT)
		{
			if (iobuf_puts(ob, "~{}~") < 0)
				return -1;
			return vbs_skip_body_of_dict(job);
		}
		else if (pv->type == VBS_LIST)
		{
			if (iobuf_puts(ob, "~[]~") < 0)
				return -1;
			return vbs_skip_body_of_list(job);
		}
	}
	else
	{
		if (pv->type == VBS_DICT || pv->type == VBS_LIST)
		{
			if (pv->descriptor)
			{
				if (iobuf_printf(ob, "%d@", pv->descriptor) < 0)
					return -1;
			}
			return _dump_to_tail(job, (pv->type == VBS_DICT), ob, kind);
		}
	}

	return vbs_print_data(pv, ob);
}

inline int vbs_unpack_print_one(vbs_unpacker_t *job, iobuf_t *ob)
{
	vbs_data_t val;
	int kind;
	int r = vbs_unpack_primitive(job, &val, &kind);
	if (r < 0)
		return r;

	return _do_unpack_print(job, ob, &val, kind);
}

int vbs_unpack_print_all(vbs_unpacker_t *job, iobuf_t *ob)
{
	if (vbs_unpack_print_one(job, ob) < 0)
		return -1;

	while (job->cur < job->end)
	{
		if (iobuf_puts(ob, "; ") != 2)
			return -1;

		if (vbs_unpack_print_one(job, ob) < 0)
			return -1;
	}

	return 0;
}

int vbs_xfmt(iobuf_t *ob, const xfmt_spec_t *spec, void *p)
{
	int ch = spec->ext.len > 4 ? spec->ext.data[4] : 0;

	if (!p)
	{
		iobuf_puts(ob, "(null)");
		return 0;
	}

	switch (ch)
	{
	case 'D':
		if (xstr_equal_cstr(&spec->ext, "VBS_DICT"))
		{
			vbs_print_dict((vbs_dict_t *)p, ob);
			return 0;
		}
		else if (xstr_equal_cstr(&spec->ext, "VBS_DATA"))
		{
			vbs_print_data((vbs_data_t *)p, ob);
			return 0;
		}
		else if (xstr_equal_cstr(&spec->ext, "VBS_DECIMAL"))
		{
			vbs_print_decimal64(*(decimal64_t *)p, ob);
			return 0;
		}
		break;

	case 'L':
		if (xstr_equal_cstr(&spec->ext, "VBS_LIST"))
		{
			vbs_print_list((vbs_list_t *)p, ob);
			return 0;
		}
		break;

	case 'S':
		if (xstr_equal_cstr(&spec->ext, "VBS_STRING"))
		{
			vbs_print_string((xstr_t *)p, ob);
			return 0;
		}
		break;

	case 'B':
		if (xstr_equal_cstr(&spec->ext, "VBS_BLOB"))
		{
			vbs_print_blob((xstr_t *)p, ob);
			return 0;
		}
		break;

	case 'R':
		if (xstr_equal_cstr(&spec->ext, "VBS_RAW"))
		{
			xstr_t *xs = (xstr_t *)p;
			vbs_unpacker_t uk = VBS_UNPACKER_INIT(xs->data, xs->len, -1);
			vbs_unpack_print_all(&uk, ob);
			return 0;
		}
		break;
	}

	return -1;
}


#ifdef TEST_VBS

#include "ostk.h"
#include "xbuf.h"

//#define PACK_NUM	(1024*1024)
//#define UNPACK_NUM	1

#define PACK_NUM	1
#define UNPACK_NUM	(1024*1024)

int main()
{
	uint8_t buf[1024];
	int i, r;
	iobuf_t ob;
	ostk_t *ostk = ostk_create(4096);
	xbuf_t xb = XBUF_INIT(buf, sizeof(buf));
	vbs_packer_t pk;
	vbs_unpacker_t uk;
	decContext ctx;

	decContextDefault(&ctx, DEC_INIT_DECIMAL64);

	r = 0;
	for (i = 0; i < PACK_NUM; ++i)
	{
		decimal64_t dec;

		xbuf_rewind(&xb);
		vbs_packer_init(&pk, xbuf_xio.write, &xb, -1);

		r |= vbs_pack_head_of_dict0(&pk);

		r |= vbs_pack_integer(&pk, LONG_MIN);
		r |= vbs_pack_cstr(&pk, "abcdefghijklmnopqrstuvwxyz");
		r |= vbs_pack_bool(&pk, 1);
		r |= vbs_pack_floating(&pk, 123456789e-300);
		r |= vbs_pack_null(&pk);

		r |= vbs_pack_head_of_dict0(&pk);

		r |= vbs_pack_cstr(&pk, "min_subnormal_double");
		r |= vbs_pack_floating(&pk, 4.9406564584124654e-324); /* Min subnormal positive double */

		r |= vbs_pack_cstr(&pk, "max_subnormal_double");
		r |= vbs_pack_floating(&pk, 2.2250738585072009e-308); /* Max subnormal double */

		r |= vbs_pack_cstr(&pk, "min_normal_double");
		r |= vbs_pack_floating(&pk, 2.2250738585072014e-308); /* Min normal positive double */

		r |= vbs_pack_cstr(&pk, "max_double");
		r |= vbs_pack_floating(&pk, 1.7976931348623157e+308); /* Max Double */

		decDoubleFromString(&dec, "123400.00E370", &ctx);

		r |= vbs_pack_cstr(&pk, "C");
		r |= vbs_pack_head_of_list0(&pk);
		r |= vbs_pack_integer(&pk, 3);
		r |= vbs_pack_decimal64(&pk, dec);
		r |= vbs_pack_tail(&pk);

		r |= vbs_pack_cstr(&pk, "D");
		r |= vbs_pack_descriptor(&pk, VBS_SPECIAL_DESCRIPTOR);
		r |= vbs_pack_head_of_list0(&pk);
		r |= vbs_pack_head_of_dict0(&pk);
		r |= vbs_pack_tail(&pk);
		r |= vbs_pack_tail(&pk);

		r |= vbs_pack_cstr(&pk, "E");
		r |= vbs_pack_cstr(&pk, "hello, world!");

		r |= vbs_pack_head_of_dict0(&pk);
		r |= vbs_pack_integer(&pk, 1);
		r |= vbs_pack_integer(&pk, 2);
		r |= vbs_pack_tail(&pk);
		r |= vbs_pack_head_of_dict0(&pk);
		r |= vbs_pack_cstr(&pk, "what?");
		r |= vbs_pack_blob(&pk, "heihei^~`;[]{}\x7f\xff, ", 18);
		r |= vbs_pack_tail(&pk);

		r |= vbs_pack_tail(&pk);

		r |= vbs_pack_tail(&pk);
		assert(r == 0);
	}
	printf("encode: r=%d len=%d\n", r, (int)xb.len);

	printf("-----------------------------------\n");
	iobuf_init(&ob, &stdio_xio, stdout, NULL, 0);

	vbs_unpacker_init(&uk, xb.data, xb.len, -1);
	r = vbs_unpack_print_all(&uk, &ob);
	iobuf_putc(&ob, '\n');
	iobuf_finish(&ob);
	printf("-----------------------------------\n");
	printf("print: r=%d\n", r);

	for (i = 0; i < UNPACK_NUM; ++i)
	{
		vbs_data_t data;

		ostk_clear(ostk);
		vbs_unpacker_init(&uk, xb.data, xb.len, -1);
		r = vbs_unpack_data(&uk, &data, &ostk_xmem, ostk);
	}
	printf("decode: r=%d p=%p end=%p\n", r, uk.cur, uk.end);

	printf("sizeof(vbs_data_t) = %zd\n", sizeof(vbs_data_t));
	return 0;
}


#endif
