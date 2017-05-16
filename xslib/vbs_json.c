#include "vbs_json.h"
#include "hex.h"
#include "xbase64.h"
#include <stdint.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: vbs_json.c,v 1.17 2015/05/28 03:19:49 gremlin Exp $";
#endif

typedef struct json_parser_t json_parser_t;

typedef int (*parse_array_body_function)(json_parser_t *parser, void *ctx);
typedef int (*parse_object_body_function)(json_parser_t *parser, void *ctx);

typedef int (*do_integer_function)(void *cookie, void *ctx, intmax_t v);
typedef int (*do_bool_function)(void *cookie, void *ctx, bool v);
typedef int (*do_floating_function)(void *cookie, void *ctx, double v);
typedef int (*do_null_function)(void *cookie, void *ctx);
typedef int (*do_head_of_string_function)(void *cookie, void *ctx, size_t len);
typedef int (*do_head_of_blob_function)(void *cookie, void *ctx, size_t len);
typedef ssize_t (*do_write_function)(void *cookie, void *ctx, const void *data, size_t len);

struct json_parser_t
{
	unsigned char *sp;
	unsigned char *end;

	parse_array_body_function	parse_array_body;
	parse_object_body_function	parse_object_body;

	void *cookie;
	do_integer_function 		do_integer;
	do_bool_function 		do_bool;
	do_floating_function 		do_floating;
	do_null_function 		do_null;
	do_head_of_string_function	do_head_of_string;
	do_head_of_blob_function 	do_head_of_blob;
	do_write_function 		do_write;
};


typedef struct
{
	const xmem_t *xm;
	void *ctx;
} memory_t;


static inline void *x_alloc(memory_t *m, size_t size)
{
	return m->xm->alloc(m->ctx, size);
}

static inline void x_free(memory_t *m, void *ptr)
{
	if (m->xm->free && ptr)
		m->xm->free(m->ctx, ptr);
}


static int _parse_string(json_parser_t *parser, void *ctx);
static int _parse_blob(json_parser_t *parser, void *ctx);
static int _parse_number(json_parser_t *parser, void *ctx);

static int _parse_primitive(json_parser_t *parser, void *ctx)
{
	int ch;
	while (parser->sp < parser->end && (ch = *parser->sp++) != 0)
	{
		switch (ch)
		{
		case '\r':
		case '\n':
		case ' ':
		case '\t':
			/* do nothing */
			break;

		case 'n':
			if (parser->sp + 3 <= parser->end 
				&& parser->sp[0] == 'u'
				&& parser->sp[1] == 'l'
				&& parser->sp[2] == 'l')
			{
				parser->sp += 3;
				if (parser->do_null(parser->cookie, ctx) < 0)
					return -1;
				return VBS_NULL;
			}
			return -1;
			break;

		case 't':
			if (parser->sp + 3 <= parser->end 
				&& parser->sp[0] == 'r'
				&& parser->sp[1] == 'u'
				&& parser->sp[2] == 'e')
			{
				parser->sp += 3;
				if (parser->do_bool(parser->cookie, ctx, true) < 0)
					return -1;
				return VBS_BOOL;
			}
			return -1;
			break;

		case 'f':
			if (parser->sp + 4 <= parser->end 
				&& parser->sp[0] == 'a'
				&& parser->sp[1] == 'l'
				&& parser->sp[2] == 's'
				&& parser->sp[3] == 'e')
			{
				parser->sp += 4;
				if (parser->do_bool(parser->cookie, ctx, false) < 0)
					return -1;
				return VBS_BOOL;
			}
			return -1;
			break;

		case '"':
			if (parser->sp + 6 <= parser->end 
				&& parser->sp[0] == '\\'
				&& parser->sp[1] == 'u'
				&& parser->sp[2] == '0'
				&& parser->sp[3] == '0'
				&& parser->sp[4] == '0'
				&& parser->sp[5] == '0')
			{
				parser->sp += 6;
				return _parse_blob(parser, ctx);
			}

			return _parse_string(parser, ctx);
			break;

		case '-':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			parser->sp--;
			return _parse_number(parser, ctx);
			break;

		case '[':
			return VBS_LIST;
			break;

		case '{':
			return VBS_DICT;
			break;

		default:
			return -1;
		}
	}

	return -1;
}

static inline int _parse_data(json_parser_t *parser, void *ctx)
{
	int type = _parse_primitive(parser, ctx);
	if (type == VBS_LIST)
		return parser->parse_array_body(parser, ctx);
	else if (type == VBS_DICT)
		return parser->parse_object_body(parser, ctx);
	return type;
}

static int _parse_blob(json_parser_t *parser, void *ctx)
{
#define BUF_SIZE	256
	char buf[BUF_SIZE];
	unsigned char *s = parser->sp;
	unsigned char *end = NULL;
	size_t len, len2;
	ssize_t n;
	int ch;

	while (parser->sp < parser->end && (ch = *parser->sp++) != 0)
	{
		if (ch == '"')
		{
			end = parser->sp - 1;
			break;
		}
		else if (std_xbase64.detab[ch] >= 64)
		{
			if (std_xbase64.detab[ch] > 64)
				return -1;

			end = parser->sp - 1;

			while (parser->sp < parser->end && std_xbase64.detab[*parser->sp] == 64)
			{
				++parser->sp;
			}

			if (parser->sp < parser->end && *parser->sp == '"')
			{
				++parser->sp;
				break;
			}

			return -1;
		}
	}

	if (!end)
		return -1;

	len = (end - s) % 4;
	if (len)
	{
		if (len == 1)
			return -1;
		--len;
	}
	len += (end - s) / 4 * 3;

	if (parser->do_head_of_blob(parser->cookie, ctx, len) < 0)
		return -1;

	len2 = 0;
	while (s + BUF_SIZE < end)
	{
		n = xbase64_decode(&std_xbase64, buf, (char *)s, BUF_SIZE, 0);
		if (n < 0 || parser->do_write(parser->cookie, ctx, buf, n) < n)
			return -1;
		len2 += n;
	}

	if (s < end)
	{
		n = xbase64_decode(&std_xbase64, buf, (char *)s, end - s, 0);
		if (n < 0 || parser->do_write(parser->cookie, ctx, buf, n) < n)
			return -1;
		len2 += n;
	}

	assert(len == len2);
	return VBS_BLOB;
#undef BUF_SIZE
}

static int _parse_string(json_parser_t *parser, void *ctx)
{
	unsigned char *s = parser->sp, *p = parser->sp;
	unsigned char *end = NULL;
	size_t len = 0, len2;
	ssize_t n;
	int ch;

	while (parser->sp < parser->end && (ch = *parser->sp++) != 0)
	{
		if (ch == '\\')
		{
			int x;
			if (parser->sp >= parser->end)
				return -1;

			x = *parser->sp;
			if (x == 'u')
			{
				unsigned char bb[2];
				uint32_t code;

				if (parser->sp + 5 > parser->end)
					return -1;

				if (unhexlify(bb, (char *)parser->sp + 1, 4) != 2)
					return -1;

				parser->sp += 5;
				code = bb[0] * 256 + bb[1];

				if (code <= 0x7F)
				{
					++len;
				}
				else if (code <= 0x7FF)
				{
					len += 2;
				}
				else if (code >= 0xd800 && code <= 0xdbff)
				{
					uint32_t low;
					if (parser->sp + 6 > parser->end)
						return -1;

					if (parser->sp[0] != '\\' || parser->sp[1] != 'u')
						return -1;

					if (unhexlify(bb, (char *)parser->sp + 2, 4) != 2)
						return -1;

					parser->sp += 6;
					low = bb[0] * 256 + bb[1];
					if (low < 0xdc00 || low > 0xdfff)
						return -1;

					len += 4;
				}
				else if (code >= 0xdc00 && code <= 0xdfff)
				{
					return -1;
				}
				else
				{
					len += 3;
				}
			}
			else if (x == 't' || x == 'r' || x == 'n' || x == 'f' || x == 'b' || x == '"' || x == '\\' || x == '/')
			{
				parser->sp++;
				++len;
			}
			else
				return -1;
		}
		else if (ch == '"')
		{
			end = parser->sp - 1;
			break;
		}
		else
		{
			++len;
		}
	}

	if (!end)
		return -1;

	if (parser->do_head_of_string(parser->cookie, ctx, len) < 0)
		return -1;

	len2 = 0;
	while (p < end)
	{
		if (*p == '\\')
		{
			unsigned char buf[4];

			if (p > s)
			{
				n = p - s;
				if (parser->do_write(parser->cookie, ctx, s, n) < n)
					return -1;
				len2 += n;
			}

			++p;
			if (*p == 'u')
			{
				unsigned char bb[2];
				uint32_t code;

				unhexlify(bb, (char *)p + 1, 4);
				code = bb[0] * 256 + bb[1];
				p += 5;

				if (code < 0x7F)
				{
					buf[0] = code;
					n = 1;
				}
				else if (code < 0x7FF)
				{
					buf[0] = (code >> 6) | 0xc0;
					buf[1] = (code & 0x3f) | 0x80;
					n = 2;
				}
				else if (code >= 0xd800 && code <= 0xdbff)
				{
					uint32_t low;

					assert(p[0] == '\\' && p[1] == 'u');
					unhexlify(bb, (char *)p + 2, 4);
					low = bb[0] * 256 + bb[1];
					p += 6;

					low -= 0xdc00;
					code -= 0xd800;
					code = ((code << 10) + low) + 0x10000;

					buf[0] = (code >> 18) | 0xf0;
					buf[1] = ((code >> 12) & 0x3f) | 0x80;
					buf[2] = ((code >> 6) & 0x3f) | 0x80;
					buf[3] = (code & 0x3f) | 0x80;
					n = 4;
				}
				else
				{
					buf[0] = (code >> 12) | 0xe0;
					buf[1] = ((code >> 6) & 0x3f) | 0x80;
					buf[2] = (code & 0x3f) | 0x80;
					n = 3;
				}
			}
			else
			{
				if (*p == 't')
				{
					buf[0] = '\t';
				}
				else if (*p == 'r')
				{
					buf[0] = '\r';
				}
				else if (*p == 'n')
				{
					buf[0] = '\n';
				}
				else if (*p == 'f')
				{
					buf[0] = '\f';
				}
				else if (*p == 'b')
				{
					buf[0] = '\b';
				}
				else
				{
					buf[0] = *p;
				}

				n = 1;
				++p;
			}

			if (parser->do_write(parser->cookie, ctx, buf, n) < n)
				return -1;
			len2 += n;
			s = p;
		}
		else
		{
			++p;
		}
	}

	if (s < end)
	{
		n = end - s;
		if (parser->do_write(parser->cookie, ctx, s, n) < n)
			return -1;
		len2 += n;
	}

	assert(len == len2);
	return VBS_STRING;
}

static int _parse_number(json_parser_t *parser, void *ctx)
{
	xstr_t xs = XSTR_INIT((unsigned char *)parser->sp, parser->end - parser->sp);
	xstr_t end;
	intmax_t n = xstr_to_integer(&xs, &end, 10);

	if (end.data == xs.data || (xs.data[0] == '-' && end.data == xs.data + 1))
		return -1;

	if (end.len && (end.data[0] == '.' || end.data[0] == 'E' || end.data[0] == 'e'))
	{
		double r = xstr_to_double(&xs, &end);
		if (parser->do_floating(parser->cookie, ctx, r) < 0)
			return -1;
		parser->sp = (unsigned char *)end.data;
		return VBS_FLOATING;
	}

	if (parser->do_integer(parser->cookie, ctx, n) < 0)
		return -1;
	parser->sp = (unsigned char *)end.data;
	return VBS_INTEGER;
}

static inline bool _is_wanted(json_parser_t *parser, char wanted)
{
	int ch;

	while (parser->sp < parser->end && (ch = *parser->sp) != 0)
	{
		if (ch == wanted)
		{
			parser->sp++;
			return true;
		}
		else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
		{
			parser->sp++;
		}
		else
			break;
	}
	return false;
}

static int _parse_array_body_to_vbs_binary(json_parser_t *parser, void *ctx)
{
	size_t n;
	bool comma = true;

	if (vbs_pack_head_of_list((vbs_packer_t *)parser->cookie) < 0)
		return -1;

	for (n = 0; true; ++n)
	{
		if (_is_wanted(parser, ']'))
			break;
		else if (!comma)
			return -1;

		if (_parse_data(parser, ctx) < 0)
			return -1;

		comma = _is_wanted(parser, ',');
	}

	if (vbs_pack_tail((vbs_packer_t *)parser->cookie) < 0)
		return -1;

	return VBS_LIST;
}

static int _parse_object_body_to_vbs_binary(json_parser_t *parser, void *ctx)
{
	size_t n;
	bool comma = true;

	if (vbs_pack_head_of_dict((vbs_packer_t *)parser->cookie) < 0)
		return -1;

	for (n = 0; true; ++n)
	{
		int type;

		if (_is_wanted(parser, '}'))
			break;
		else if (!comma)
			return -1;

		type = _parse_data(parser, ctx);

		if (type != VBS_STRING)
			return -1;

		if (!_is_wanted(parser, ':'))
			return -1;

		if (_parse_data(parser, ctx) < 0)
			return -1;

		comma = _is_wanted(parser, ',');
	}

	if (vbs_pack_tail((vbs_packer_t *)parser->cookie) < 0)
		return -1;

	return VBS_DICT;
}

static int _pk_integer(vbs_packer_t *pk, void *ctx, intmax_t v)
{
	return vbs_pack_integer(pk, v);
}

static int _pk_bool(vbs_packer_t *pk, void *ctx, bool v)
{
	return vbs_pack_bool(pk, v);
}

static int _pk_floating(vbs_packer_t *pk, void *ctx, double v)
{
	return vbs_pack_floating(pk, v);
} 

static int _pk_null(vbs_packer_t *pk, void *ctx)
{
	return vbs_pack_null(pk);
}

static int _pk_head_of_string(vbs_packer_t *pk, void *ctx, size_t len)
{
	return vbs_pack_head_of_string(pk, len);
}

static int _pk_head_of_blob(vbs_packer_t *pk, void *ctx, size_t len)
{
	return vbs_pack_head_of_blob(pk, len);
}

static ssize_t _pk_write(vbs_packer_t *pk, void *ctx, const void *data, size_t len)
{
	return pk->write(pk->cookie, data, len);
}

ssize_t json_to_vbs(const void *json, size_t size, xio_write_function xio_write, void *xio_ctx, int flags)
{
	json_parser_t parser;
	vbs_packer_t pk = VBS_PACKER_INIT(xio_write, xio_ctx, -1);
	int type;

	parser.sp = (unsigned char *)json;
	parser.end = parser.sp + size;
	if (parser.end < parser.sp)
		parser.end = (unsigned char *)-1;

	parser.parse_array_body = _parse_array_body_to_vbs_binary;
	parser.parse_object_body = _parse_object_body_to_vbs_binary;

	parser.cookie = &pk;
#define FUNC(T)		parser.do_##T = (do_##T##_function)_pk_##T
	FUNC(integer);
	FUNC(bool);
	FUNC(floating);
	FUNC(null);
	FUNC(head_of_string);
	FUNC(head_of_blob);
	FUNC(write);
#undef FUNC

	type = _parse_data(&parser, NULL);
	if (type < 0)
		return -1;

	if (pk.error || pk.depth)
		return -1;

	return parser.sp - (unsigned char *)json;
}

static int _unpack_array_body_as_list(json_parser_t *parser, vbs_list_t *list)
{
	memory_t *mm = (memory_t *)parser->cookie;
	size_t n;
	bool comma = true;

	for (n = 0; true; ++n)
	{
		vbs_litem_t *ent;
		if (_is_wanted(parser, ']'))
			break;
		else if (!comma)
			return -1;

		ent = (vbs_litem_t *)x_alloc(mm, sizeof(*ent));
		if (!ent)
			return -1;

		vbs_litem_init(ent);
		vbs_list_push_back(list, ent);
		if (_parse_data(parser, &ent->value) < 0)
			return -1;

		comma = _is_wanted(parser, ',');
	}

	return VBS_LIST;
}

static int _parse_array_body_to_vbs_data(json_parser_t *parser, void *ctx)
{
	memory_t *mm = (memory_t *)parser->cookie;
	vbs_data_t *pv = (vbs_data_t *)ctx;

	pv->type = VBS_LIST;
	pv->d_list = (vbs_list_t *)x_alloc(mm, sizeof(*pv->d_list));
	if (!pv->d_list)
		return -1;
	vbs_list_init(pv->d_list);
	return _unpack_array_body_as_list(parser, pv->d_list);
}

static int _unpack_object_body_as_dict(json_parser_t *parser, vbs_dict_t *dict)
{
	memory_t *mm = (memory_t *)parser->cookie;
	size_t n;
	bool comma = true;

	for (n = 0; true; ++n)
	{
		vbs_ditem_t *ent;
		int type;

		if (_is_wanted(parser, '}'))
			break;
		else if (!comma)
			return -1;

		ent = (vbs_ditem_t *)x_alloc(mm, sizeof(*ent));
		if (!ent)
			return -1;
		vbs_ditem_init(ent);
		vbs_dict_push_back(dict, ent);

		type = _parse_data(parser, &ent->key);

		if (type != VBS_STRING)
			return -1;

		if (!_is_wanted(parser, ':'))
			return -1;

		if (_parse_data(parser, &ent->value) < 0)
			return -1;

		comma = _is_wanted(parser, ',');
	}

	return VBS_DICT;
}

static int _parse_object_body_to_vbs_data(json_parser_t *parser, void *ctx)
{
	memory_t *mm = (memory_t *)parser->cookie;
	vbs_data_t *pv = (vbs_data_t *)ctx;

	pv->type = VBS_DICT;
	pv->d_dict = (vbs_dict_t *)x_alloc(mm, sizeof(*pv->d_dict));
	if (!pv->d_dict)
		return -1;
	vbs_dict_init(pv->d_dict);
	return _unpack_object_body_as_dict(parser, pv->d_dict);
}

static int _v_integer(memory_t *mm, vbs_data_t *pv, intmax_t v)
{
	pv->type = VBS_INTEGER;
	pv->d_int = v;
	return 0;
}

static int _v_bool(memory_t *mm, vbs_data_t *pv, bool v)
{
	pv->type = VBS_BOOL;
	pv->d_bool = v;
	return 0;
}

static int _v_floating(memory_t *mm, vbs_data_t *pv, double v)
{
	pv->type = VBS_FLOATING;
	pv->d_floating = v;
	return 0;
}

static int _v_null(memory_t *mm, vbs_data_t *pv)
{
	pv->type = VBS_NULL;
	return 0;
}

static int _v_head_of_string(memory_t *mm, vbs_data_t *pv, size_t len)
{
	pv->type = VBS_STRING;
	pv->is_owner = true;
	pv->d_xstr.data = x_alloc(mm, len+1);
	pv->d_xstr.data[len] = 0;
	pv->d_xstr.len = 0;
	return 0;
}

static int _v_head_of_blob(memory_t *mm, vbs_data_t *pv, size_t len)
{
	pv->type = VBS_BLOB;
	pv->is_owner = true;
	pv->d_blob.data = x_alloc(mm, len+1);
	pv->d_blob.data[len] = 0;
	pv->d_blob.len = 0;
	return 0;
}

static int _v_write(memory_t *mm, vbs_data_t *pv, const void *src, size_t len)
{
	if (pv->type == VBS_STRING)
	{
		memcpy(pv->d_xstr.data + pv->d_xstr.len, src, len);
		pv->d_xstr.len += len;
	}
	else if (pv->type == VBS_BLOB)
	{
		memcpy(pv->d_blob.data + pv->d_blob.len, src, len);
		pv->d_blob.len += len;
	}
	else
	{
		assert(!"can't reach here!");
	}
	return len;
}

static void _init_parser(json_parser_t *parser, const void *json, size_t size, memory_t *mm)
{
	parser->sp = (unsigned char *)json;
	parser->end = parser->sp + size;
	if (parser->end < parser->sp)
		parser->end = (unsigned char *)-1;

	parser->parse_array_body = _parse_array_body_to_vbs_data;
	parser->parse_object_body = _parse_object_body_to_vbs_data;

	parser->cookie = mm;
#define FUNC(T)		parser->do_##T = (do_##T##_function)_v_##T
	FUNC(integer);
	FUNC(bool);
	FUNC(floating);
	FUNC(null);
	FUNC(head_of_string);
	FUNC(head_of_blob);
	FUNC(write);
#undef FUNC
}

ssize_t json_unpack_vbs_data(const void *json, size_t size, vbs_data_t *data, const xmem_t *xm, void *xm_cookie)
{
	json_parser_t parser;
	memory_t mm = { xm, xm_cookie };
	int type;

	if (!xm)
		mm.xm = &stdc_xmem;

	vbs_data_init(data);
	_init_parser(&parser, json, size, &mm);

	type = _parse_data(&parser, data);
	if (type < 0)
		goto error;

	return parser.sp - (unsigned char *)json;
error:
	vbs_release_data(data, xm, xm_cookie);
	return -1;
}

ssize_t json_unpack_vbs_list(const void *json, size_t size, vbs_list_t *list, const xmem_t *xm, void *xm_cookie)
{
	json_parser_t parser;
	memory_t mm = { xm, xm_cookie };
	vbs_data_t data;
	int type;

	if (!xm)
		mm.xm = &stdc_xmem;

	vbs_list_init(list);
	_init_parser(&parser, json, size, &mm);

	type = _parse_primitive(&parser, &data);
	if (type != VBS_LIST)
		goto error;

	type = _unpack_array_body_as_list(&parser, list);
	if (type < 0)
		goto error;

	return parser.sp - (unsigned char *)json;
error:
	vbs_release_list(list, xm, xm_cookie);
	return -1;
}

ssize_t json_unpack_vbs_dict(const void *json, size_t size, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie)
{
	json_parser_t parser;
	memory_t mm = { xm, xm_cookie };
	vbs_data_t data;
	int type;

	if (!xm)
		mm.xm = &stdc_xmem;

	vbs_dict_init(dict);
	_init_parser(&parser, json, size, &mm);

	type = _parse_primitive(&parser, &data);
	if (type != VBS_DICT)
		goto error;

	type = _unpack_object_body_as_dict(&parser, dict);
	if (type < 0)
		goto error;

	return parser.sp - (unsigned char *)json;
error:
	vbs_release_dict(dict, xm, xm_cookie);
	return -1;
}



typedef struct
{
	xio_write_function write;
	void *ctx;
} writer_t;

static inline ssize_t x_write(writer_t *w, const void *data, size_t size)
{
	return w->write(w->ctx, data, size);
}

static inline ssize_t x_putc(writer_t *w, char ch)
{
	return w->write(w->ctx, &ch, 1);
}


static int _vbs_unpack_to_list_tail(vbs_unpacker_t *job, writer_t *wr, int flags);
static int _vbs_unpack_to_dict_tail(vbs_unpacker_t *job, writer_t *wr, int flags);

static uint32_t _escape_meta[8] = {
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0x00008004, /* 0000 0000 0000 0000  1000 0000 0000 0100 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0x10000000, /* 0001 0000 0000 0000  0000 0000 0000 0000 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */

	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
	0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
#if 0
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
#endif
};
#define ISMETA(ch)	(_escape_meta[(unsigned char)(ch) >> 5] & (1 << ((ch) & 0x1f)))

static unsigned char _escape_tab[0x20] = {
	0,   0,   0,   0,   0,   0,   0,   0,
	'b', 't', 'n', 0,   'f', 'r', 0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
	0,   0,   0,   0,   0,   0,   0,   0,
};

static int _print_string(writer_t *wr, const xstr_t *xs, int flags)
{
	bool escape_unicode = (flags & VBS_JSON_ESCAPE_UNICODE);
	size_t pos = 0;
	xstr_t s = *xs;

	if (x_putc(wr, '"') < 1)
		return -1;

	while (pos < s.len)
	{
		int ch = (unsigned char)s.data[pos];
		if (ISMETA(ch) && (escape_unicode || ch < 0x80))
		{
			char esc[12];
			unsigned char x;
			int esc_len = 0;

			if (pos)
			{
				if (x_write(wr, s.data, pos) < pos)
					return -1;
			}

			if (ch == '"' || ch == '\\' || ch == '/')
			{
				esc[0] = '\\';
				esc[1] = ch;
				esc_len = 2;
				++pos;
			}
			else if (ch >= 0x80)
			{
				int code, b1, b2, b3;

				if (ch < 0xe0)
				{
					if (ch < 0xc2 || pos + 2 > s.len)
						return -1;
					pos++;
					b1 = (unsigned char)s.data[pos++];
					if (b1 < 0x80 || b1 >= 0xc0)
						return -1;
					code = ((ch & 0x1f) << 6) | (b1 & 0x3f);

					esc[0] = '\\';
					esc[1] = 'u';
					sprintf(esc + 2, "%04X", code);
					esc_len = 6;
				}
				else if (ch < 0xf0)
				{
					if (pos + 3 > s.len)
						return -1;
					pos++;
					b1 = (unsigned char)s.data[pos++];
					if (b1 < 0x80 || b1 >= 0xc0)
						return -1;
					b2 = (unsigned char)s.data[pos++];
					if (b2 < 0x80 || b2 >= 0xc0)
						return -1;
					code = ((ch & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f);

					esc[0] = '\\';
					esc[1] = 'u';
					sprintf(esc + 2, "%04X", code);
					esc_len = 6;
				}
				else
				{
					if (ch >= 0xf5 || pos + 4 > s.len)
						return -1;
					pos++;
					b1 = (unsigned char)s.data[pos++];
					if (b1 < 0x80 || b1 >= 0xc0)
						return -1;
					b2 = (unsigned char)s.data[pos++];
					if (b2 < 0x80 || b2 >= 0xc0)
						return -1;
					b3 = (unsigned char)s.data[pos++];
					if (b3 < 0x80 || b3 >= 0xc0)
						return -1;
					code = ((ch & 0x0f) << 18) | ((b1 & 0x3f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
					code -= 0x10000;

					esc[0] = '\\';
					esc[1] = 'u';
					sprintf(esc + 2, "%04X", 0xD800 + (code >> 10));
					esc[6] = '\\';
					esc[7] = 'u';
					sprintf(esc + 8, "%04X", 0xDC00 + (code & 0x3FF));
					esc_len = 12;
				}
			}
			else if ((x = _escape_tab[ch]) != 0)
			{
				esc[0] = '\\';
				esc[1] = x;
				esc_len = 2;
				++pos;
			}
			else
			{
				unsigned char b1 = ch >> 4;
				unsigned char b2 = ch & 0x0F;

				esc[0] = '\\';
				esc[1] = 'u';
				esc[2] = '0';
				esc[3] = '0';
				esc[4] = b1 < 10 ? (b1 + '0') : (b1 - 10 + 'A');
				esc[5] = b2 < 10 ? (b2 + '0') : (b2 - 10 + 'A');
				esc_len = 6;
				++pos;
			}

			if (x_write(wr, esc, esc_len) < esc_len)
				return -1;

			s.data += pos;
			s.len -= pos;
			pos = 0;
		}
		else
		{
			++pos;
		}
	}

	if (s.len)
	{
		if (x_write(wr, s.data, s.len) < s.len)
			return -1;
	}

	if (x_putc(wr, '"') < 1)
		return -1;

	return 0;
}

static int _print_blob(writer_t *wr, const xstr_t *xs)
{
#define BLOCK_SIZE	192
	char buf[XBASE64_LEN(BLOCK_SIZE) + 1];
	xstr_t s = *xs;
	ssize_t n;

	if (x_write(wr, "\"\\u0000", 7) < 7)
		return -1;

	while (s.len > BLOCK_SIZE)
	{
		n = xbase64_encode(&std_xbase64, buf, s.data, BLOCK_SIZE, XBASE64_NO_PADDING);
		xstr_advance(&s, BLOCK_SIZE);
		if (x_write(wr, buf, n) < n)
			return -1;
	}

	if (s.len)
	{
		n = xbase64_encode(&std_xbase64, buf, s.data, s.len, XBASE64_NO_PADDING);
		if (x_write(wr, buf, n) < n)
			return -1;
	}

	if (x_putc(wr, '"') < 1)
		return -1;

	return 0;
#undef BLOCK_SIZE
}

static int _vbs_to_json(vbs_unpacker_t *job, writer_t *wr, int flags, bool must_string)
{
	ssize_t r, n;
	vbs_data_t v;
	char buf[32];

	if (vbs_unpack_primitive(job, &v, NULL) < 0)
		return -1;

	if (must_string && v.type != VBS_STRING)
		return -1;

	switch (v.type)
	{
	case VBS_INTEGER:
		n = snprintf(buf, sizeof(buf), "%jd", v.d_int);
		r = x_write(wr, buf, n);
		break;
	case VBS_STRING:
		r = _print_string(wr, &v.d_xstr, flags);
		break;
	case VBS_BOOL:
		r = v.d_bool ? x_write(wr, "true", 4) : x_write(wr, "false", 5);
		break;
	case VBS_FLOATING:
		n = snprintf(buf, sizeof(buf), "%#.17G", v.d_floating);
		if (buf[n-1] == '.')
			buf[n++] = '0';
		r = x_write(wr, buf, n);
		break;
	case VBS_BLOB:
		r = _print_blob(wr, &v.d_blob);
		break;
	case VBS_NULL:
		r = x_write(wr, "null", 4);
		break;
	case VBS_DICT:
		r = _vbs_unpack_to_dict_tail(job, wr, flags);
		break;
	case VBS_LIST:
		r = _vbs_unpack_to_list_tail(job, wr, flags);
		break;
	default:
		r = -1;
	}
	return r < 0 ? -1 : 0;
}


ssize_t vbs_to_json(const void *vbs, size_t size, xio_write_function xio_write, void *xio_ctx, int flags)
{
	vbs_unpacker_t uk = VBS_UNPACKER_INIT((unsigned char *)vbs, size, -1);
	writer_t writer = { xio_write, xio_ctx };
	if (_vbs_to_json(&uk, &writer, flags, false) < 0)
		return -1;
	return uk.cur - uk.buf;
}

static int _vbs_unpack_to_list_tail(vbs_unpacker_t *job, writer_t *wr, int flags)
{
	size_t n;

	if (x_putc(wr, '[') < 1)
		return -1;

	for (n = 0; true; ++n)
	{
		if (vbs_unpack_if_tail(job))
			break;

		if (n > 0)
		{
			if (x_putc(wr, ',') < 1)
				return -1;
		}

		if (_vbs_to_json(job, wr, flags, false) < 0)
			return -1;
	}

	if (x_putc(wr, ']') < 1)
		return -1;

	return 0;
}

static int _vbs_unpack_to_dict_tail(vbs_unpacker_t *job, writer_t *wr, int flags)
{
	size_t n;

	if (x_putc(wr, '{') < 1)
		return -1;

	for (n = 0; true; ++n)
	{
		if (vbs_unpack_if_tail(job))
			break;

		if (n > 0)
		{
			if (x_putc(wr, ',') < 1)
				return -1;
		}

		if (_vbs_to_json(job, wr, flags, true) < 0)
			return -1;

		if (x_putc(wr, ':') < 1)
			return -1;

		if (_vbs_to_json(job, wr, flags, false) < 0)
			return -1;
	}

	if (x_putc(wr, '}') < 1)
		return -1;

	return 0;
}

static int _print_vbs_list(writer_t *wr, const vbs_list_t *list, int flags);
static int _print_vbs_dict(writer_t *wr, const vbs_dict_t *dict, int flags);

static int _print_vbs_data(writer_t *wr, const vbs_data_t *v, int flags)
{
	ssize_t r, n;
	char buf[32];

	switch (v->type)
	{
	case VBS_INTEGER:
		n = snprintf(buf, sizeof(buf), "%jd", v->d_int);
		r = x_write(wr, buf, n);
		break;
	case VBS_STRING:
		r = _print_string(wr, &v->d_xstr, flags);
		break;
	case VBS_BOOL:
		r = v->d_bool ? x_write(wr, "true", 4) : x_write(wr, "false", 5);
		break;
	case VBS_FLOATING:
		n = snprintf(buf, sizeof(buf), "%#.17G", v->d_floating);
		if (buf[n-1] == '.')
			buf[n++] = '0';
		r = x_write(wr, buf, n);
		break;
	case VBS_BLOB:
		r = _print_blob(wr, &v->d_blob);
		break;
	case VBS_NULL:
		r = x_write(wr, "null", 4);
		break;
	case VBS_DICT:
		r = _print_vbs_dict(wr, v->d_dict, flags);
		break;
	case VBS_LIST:
		r = _print_vbs_list(wr, v->d_list, flags);
		break;
	default:
		r = -1;
	}
	return r < 0 ? -1 : 0;
}

static int _print_vbs_list(writer_t *wr, const vbs_list_t *list, int flags)
{
	vbs_litem_t *ent;

	if (x_putc(wr, '[') < 1)
		return -1;

	for (ent = list->first; ent; ent = ent->next)
	{
		if (ent != list->first)
		{
			if (x_putc(wr, ',') < 1)
				return -1;
		}

		if (_print_vbs_data(wr, &ent->value, flags) < 0)
			return -1;
	}

	if (x_putc(wr, ']') < 1)
		return -1;
	return 0;
}

static int _print_vbs_dict(writer_t *wr, const vbs_dict_t *dict, int flags)
{
	vbs_ditem_t *ent;

	if (x_putc(wr, '{') < 1)
		return -1;

	for (ent = dict->first; ent; ent = ent->next)
	{
		if (ent != dict->first)
		{
			if (x_putc(wr, ';') < 1)
				return -1;
		}

		if (_print_vbs_data(wr, &ent->key, flags) < 0)
			return -1;
		if (x_putc(wr, ':') < 1)
			return -1;
		if (_print_vbs_data(wr, &ent->value, flags) < 0)
			return -1;
	}

	if (x_putc(wr, '}') < 1)
		return -1;
	return 0;
}

int json_pack_vbs_data(const vbs_data_t *data, xio_write_function xio_write, void *xio_ctx, int flags)
{
	writer_t wr = { xio_write, xio_ctx };
	return _print_vbs_data(&wr, data, flags);
}

int json_pack_vbs_list(const vbs_list_t *list, xio_write_function xio_write, void *xio_ctx, int flags)
{
	writer_t wr = { xio_write, xio_ctx };
	return _print_vbs_list(&wr, list, flags);
}

int json_pack_vbs_dict(const vbs_dict_t *dict, xio_write_function xio_write, void *xio_ctx, int flags)
{
	writer_t wr = { xio_write, xio_ctx };
	return _print_vbs_dict(&wr, dict, flags);
}


#ifdef TEST_VBS_JSON

#include "ostk.h"

int main(int argc, char **argv)
{
	xstr_t xs = XSTR_CONST(" { \"B\":\"\\u0000VGhpcyBpcyBhIGJsb2I=\", "
		"\"\\u4f60\\u597d\\ud834\\udd1e\" : 12345 , "
		"\"hello\" : -12345E+0, "
		"\"w\torld\x07\":[true,false,null , ], } ");
	char buf[1024];
	char *p = buf;
	vbs_dict_t dict;

	json_to_vbs(xs.data, xs.len, pptr_xio.write, &p, 0);
	vbs_to_json(buf, p - buf, stdio_xio.write, stdout, 0);
	fprintf(stdout, "\n");

	json_unpack_vbs_dict(xs.data, xs.len, &dict, NULL, NULL);
	json_pack_vbs_dict(&dict, stdio_xio.write, stdout, VBS_JSON_ESCAPE_UNICODE);
	fprintf(stdout, "\n");

	vbs_release_dict(&dict, NULL, NULL);

	return 0;
}

#endif
