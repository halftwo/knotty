#include "vbs_pack.h"
#include "floating.h"
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>


#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: vbs_pack.c,v 1.57 2015/06/11 02:36:29 gremlin Exp $";
#endif

#define TMPBUF_SIZE	24

#ifndef SSIZE_MAX
#define SSIZE_MAX	(SIZE_MAX/2)
#endif

#define FLOATING_ZERO_BACKWARD_COMPATIBLE	0

enum
{
	FLT_ZERO_ZERO	=  0,		/*  0.0 for backward compatibility */
	FLT_ZERO	=  1,		/* +0.0 */
	FLT_INF		=  2,		/* +infinite */
	FLT_NAN		=  3,		/* +NaN */
	FLT_SNAN	=  4,		/* signalling NaN */
};

const xstr_t vbs_packed_empty_list = XSTR_CONST("\x02\x01");
const xstr_t vbs_packed_empty_dict = XSTR_CONST("\x03\x01");

static const char *_tpnames[] = 
{
	"ERR",		/*  0 */
	"TAIL",		/*  1 */
	"LIST",		/*  2 */
	"DICT",		/*  3 */
	"NULL",		/*  4 */
	"FLOATING",	/*  5 */
	"DECIMAL",	/*  6 */
	"BOOL",		/*  7 */
	"STRING",	/*  8 */
	"INTEGER",	/*  9 */
	"BLOB",		/* 10 */
	"DESCRIPTOR",	/* 11 */
};

static unsigned char _tpidx[VBS_INTEGER + 1] = 
{
	 0,  1,  2,  3,  0,  0,  0, 0,	0,  0,  0,  0,  0,  0,  0,  4,
	11,  0,  0,  0,  0,  0,  0, 0, 	7,  0,  0, 10,  6,  0,  5,  0,
	 8,  0,  0,  0,  0,  0,  0, 0, 	0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0, 0, 	0,  0,  0,  0,  0,  0,  0,  0,
	 9,
};

const char *vbs_kind_name(vbs_kind_t kind)
{
	return (kind >= 0 && kind <= VBS_INTEGER) ? _tpnames[_tpidx[kind]] : _tpnames[0];
}

static inline size_t _pack_intstr(unsigned char *buf, int kind, uintmax_t num)
{
	unsigned char *p = buf;
	while (num >= 0x20)
	{
		*p++ = 0x80 | num;
		num >>= 7;
	}

	*p++ = kind | num;
	return (p - buf);
}

static inline size_t _intstr_size(uintmax_t num)
{
	size_t n = 1;
	if (num >= 0x20)
	{
		do
		{
			num >>= 7;
			++n;
		} while (num >= 0x20);
	}
	return n;
}

static inline size_t _pack_tag(unsigned char *buf, int kind, uintmax_t num)
{
	unsigned char *p = buf;
	if (num)
	{
		do
		{
			*p++ = 0x80 | num;
			num >>= 7;
		} while (num);
	}

	*p++ = kind;
	return (p - buf);
}

static inline size_t _tag_size(uintmax_t num)
{
	size_t n = 1;
	if (num)
	{
		do
		{
			num >>= 7;
			++n;
		} while (num);
	}
	return n;
}

/* The MSB is position 63 (or 31), the LSB is position 0. */
static inline int _find_last_bit_set(uintmax_t v)
{
	int r = -1;
	while (v != 0)
	{
		v >>= 1;
		++r;
	}

	return r;
}

/* See: 
 * 	http://en.wikipedia.org/wiki/IEEE_floating_point
 * 	http://en.wikipedia.org/wiki/Double-precision_floating-point_format
 */
static void _break_double_value(double value, intmax_t *p_significand, int *p_expo)
{
	union ieee754_binary64 v;
	int64_t significand;
	int expo;
	bool negative;

	v.d = value;
	significand = ((uint64_t)v.ieee.mantissa0 << 32) + v.ieee.mantissa1;
	negative = v.ieee.negative;

	if (v.ieee.exponent == 0x7ff)
	{
		expo = significand ? FLT_NAN : FLT_INF;
		significand = 0;
	}
	else if (v.ieee.exponent == 0)
	{
		if (significand == 0)
		{
#if FLOATING_ZERO_BACKWARD_COMPATIBLE
			expo = FLT_ZERO_ZERO;
#else
			expo = FLT_ZERO;
#endif
		}
		else
			expo = 1 - IEEE754_BINARY64_BIAS;
	}
	else
	{
		expo = v.ieee.exponent - IEEE754_BINARY64_BIAS;
		significand |= ((int64_t)1 << 52);
	}

	if (significand)
	{
		int shift = 0;
		while ((significand & 0x01) == 0)
		{
			significand >>= 1;
			++shift;
		}
		expo = expo - 52 + shift;
		*p_significand = negative ? -significand : significand;
		*p_expo = expo;
	}
	else
	{
		*p_significand = 0;
		*p_expo = negative ? -expo : expo;
	}
}

int vbs_make_double_value(double* value, intmax_t significand, int expo)
{
	union ieee754_binary64 v;

	v.d = +0.0;
	if (significand)
	{
		int point;
		int shift;

		if (significand < 0)
		{
			v.ieee.negative = 1;
			significand = -significand;
		}

		point = _find_last_bit_set(significand);
		expo += IEEE754_BINARY64_BIAS + point;

		if (expo >= 0x7ff)
		{
			v.ieee.exponent = 0x7ff;
		}
		else
		{
			uint64_t mantissa;
			if (expo <= 0)
			{
				v.ieee.exponent = 0;
				shift = 52 - (point + 1) + expo;
			}
			else
			{
				v.ieee.exponent = expo;
				shift = 52 - point;
			}

			mantissa = (shift >= 0) ? (significand << shift) : (significand >> -shift);
			v.ieee.mantissa0 = mantissa >> 32;
			v.ieee.mantissa1 = mantissa;
		}
	}
	else
	{
		if (expo < 0)
		{
			v.ieee.negative = 1;
			expo = -expo;
		}

		if (expo <= FLT_ZERO)
		{
			v.ieee.exponent = 0;
		}
		else if (expo == FLT_INF)
		{
			v.ieee.exponent = 0x7ff;
		}
		else /* NaN */
		{
			v.ieee.exponent = 0x7ff;
			v.ieee.mantissa0 = 0x000fffffUL;
			v.ieee.mantissa1 = 0xffffffffUL;
		}
	}

	*value = v.d;
	return 0;
}

static void _break_decimal64_value(decimal64_t value, intmax_t *p_significand, int *p_expo)
{
	uint8_t bcd[DECDOUBLE_Pmax];
	int32_t expo, sign;
	intmax_t significand;
	int ep;

	sign = decDoubleToBCD(&value, &expo, bcd);

	if (expo > DECDOUBLE_Emax || expo < -DECDOUBLE_Bias)
	{
		significand = 0;

		if (expo == DECFLOAT_Inf)
			ep = FLT_INF;
		else if (expo == DECFLOAT_qNaN)
			ep = FLT_NAN;
		else if (expo == DECFLOAT_sNaN)
			ep = FLT_SNAN;
		else
			ep = FLT_SNAN;
	}
	else
	{
		int i, n;
		for (i = 0; i < DECDOUBLE_Pmax && (bcd[i] == 0); ++i)
		{
			continue;
		}

		for (n = DECDOUBLE_Pmax; n > i && (bcd[n-1] == 0); --n)
		{
			++expo;
		}

		significand = 0;
		for (; i < n; ++i)
		{
			significand *= 10;
			significand += bcd[i];
		}

		ep = significand ? expo : FLT_ZERO;
	}

	if (significand)
	{
		*p_significand = sign ? -significand : significand;
		*p_expo = ep;
	}
	else
	{
		*p_significand = 0;
		*p_expo = sign ? -ep : ep;
	}
}

static bool is_zero(uint8_t *bytes, size_t n)
{
	while (n--)
	{
		if (bytes[n])
			return false;
	}
	return true;
}

int vbs_make_decimal64_value(decimal64_t *value, intmax_t significand, int ep)
{
	uint8_t bcd[DECDOUBLE_Pmax];
	int32_t expo, sign = 0;

	memset(bcd, 0, sizeof(bcd));
	if (significand)
	{
		int i, digits;
		if (significand < 0)
		{
			sign = DECFLOAT_Sign;
			significand = -significand;
		}

		for (i = DECDOUBLE_Pmax; i > 0 && significand; --i)
		{
			bcd[i-1] = significand % 10;
			significand /= 10;
		}

		if (significand)
		{
			// NB: we may be losing digits.
			do {
				memmove(bcd + 1, bcd, sizeof(bcd) - 1);
				bcd[0] = significand % 10;
				significand /= 10;
				++ep;
			} while (significand);
		}

		/* MAX: 9.999999999999999E+384
		 * MIN: 0.000000000000001E-383 = 1E-398
		 */

		digits = DECDOUBLE_Pmax - i;

		if (ep + digits - 1 > DECDOUBLE_Emax)
		{
			memset(bcd, 0, sizeof(bcd));
			expo = DECFLOAT_Inf;
		}
		else if (ep + digits - 1 < -DECDOUBLE_Bias)
		{
			if (ep + digits == -DECDOUBLE_Bias && 
				(bcd[i] > 5 || (bcd[i] == 5 && !is_zero(bcd+i+1, digits-1))))
			{
				memset(bcd, 0, sizeof(bcd));
				bcd[DECDOUBLE_Pmax - 1] = 1;
				expo = -DECDOUBLE_Bias;
			}
			else
			{
				memset(bcd, 0, sizeof(bcd));
				expo = 0;
			}
		}
		else
		{
			if (ep > DECDOUBLE_Emax - (DECDOUBLE_Pmax - 1))
			{
				memmove(bcd, bcd + i, digits);
				memset(bcd + digits, 0, i);
				ep -= i;
			}

			expo = ep;
		}

		decDoubleFromBCD(value, expo, bcd, sign);
	}
	else
	{
		if (ep < 0)
		{
			sign = DECFLOAT_Sign;
			ep = -ep;
		}

		if (ep <= FLT_ZERO)
		{
			expo = 0;
		}
		else if (ep == FLT_INF)
		{
			expo = DECFLOAT_Inf;
		}
		else if (ep == FLT_NAN)
		{
			expo = DECFLOAT_qNaN;
		}
		else /* sNaN */
		{
			expo = DECFLOAT_sNaN;
		}

		decDoubleFromBCD(value, expo, bcd, sign);
	}

	return 0;
}

static inline bool _unpacker_size_ok(vbs_unpacker_t *job, intmax_t num)
{
	return (num >= 0) && (num <= job->end - job->cur);
}

static const bset_t _single_byte_bset = 
{
	{
	0xFB00C00E, /* 1111 1011 1111 1111  1000 0000 0000 1110 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xFFFFFFFF, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0xFFFFFFFF, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0xFFFFFFFF, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

static const bset_t _multi_byte_bset = 
{
	{
	0xF800400C, /* 1111 1000 1111 1111  0000 0000 0000 1100 */

		    /* ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
	0xFFFFFFFF, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /* _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
	0xFFFFFFFF, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

		    /*  ~}| {zyx wvut srqp  onml kjih gfed cba` */
	0xFFFFFFFF, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	0x00000000, /* 0000 0000 0000 0000  0000 0000 0000 0000 */
	}
};

vbs_kind_t vbs_unpack_kind(vbs_unpacker_t *job, intmax_t *p_num)
{
	int kind;
	int descriptor = 0;
	bool negative = false;
	uint8_t *p = job->cur;
again:
	if (p < job->end)
	{
		uintmax_t num = 0;
		int x = *(uint8_t *)p++;
		if (x < 0x80)
		{
			kind = x;
			if (x >= VBS_STRING)
			{
				kind = (x & 0x60);
				num = (x & 0x1F);
				if (kind == 0x60)
				{
					kind = VBS_INTEGER;
					negative = true;
				}
			}
			else if (x >= VBS_BOOL)
			{
				if (x != VBS_BLOB)
					kind = (x & 0xFE);

				if (x <= VBS_BOOL + 1)
					num = (x & 0x01);

				/* For VBS_DECIMAL and VBS_FLOATING, the negative bit
				 * has no effect when num == 0.
 				 * So we ignore it.
 				 */

				/* negative = (x & 0x01); */
			}
			else if (x >= VBS_DESCRIPTOR)
			{
				num = (x & 0x07);
				if (num == 0)
				{
					if ((descriptor & VBS_SPECIAL_DESCRIPTOR) == 0)
						descriptor |= VBS_SPECIAL_DESCRIPTOR;
					else
						return VBS_ERR_INVALID;
				}
				else
				{
					if ((descriptor & VBS_DESCRIPTOR_MAX) == 0)
						descriptor |= num;
					else
						return VBS_ERR_INVALID;
				}
				goto again;
			}
			else if (!BSET_TEST(&_single_byte_bset, x))
			{
				job->error = __LINE__;
				return VBS_ERR_INVALID;
			}
		}
		else
		{
			int left, shift;

			if (p >= job->end)
			{
				job->error = __LINE__;
				return VBS_ERR_INCOMPLETE;
			}

			shift = 7;
			num = x & 0x7F;
			for (x = *(uint8_t *)p++; x >= 0x80; x = *(uint8_t *)p++, shift += 7)
			{
				if (p >= job->end)
				{
					job->error = __LINE__;
					return VBS_ERR_INCOMPLETE;
				}
				x &= 0x7F;
				left = sizeof(uintmax_t) * 8 - shift;
				if (left <= 0 || (left < 7 && x >= (1 << left)))
				{
					job->error = __LINE__;
					return VBS_ERR_TOOBIG;
				}
				num |= ((uintmax_t)x) << shift;
			}

			kind = x;
			if (x >= VBS_STRING)
			{
				kind = (x & 0x60);
				x &= 0x1F;
				if (x)
				{
					left = sizeof(uintmax_t) * 8 - shift;
					if (left <= 0 || (left < 7 && x >= (1 << left)))
					{
						job->error = __LINE__;
						return VBS_ERR_TOOBIG;
					}
					num |= ((uintmax_t)x) << shift;
				}
				if (kind == 0x60)
				{
					kind = VBS_INTEGER;
					negative = true;
				}
			}
			else if (x >= VBS_DECIMAL)
			{
				kind = (x & 0xFE);
				negative = (x & 0x01);
			}
			else if (x >= VBS_DESCRIPTOR && x < VBS_BOOL)
			{
				x &= 0x07;
				if (x)
				{
					left = sizeof(uintmax_t) * 8 - shift;
					if (left <= 0 || (left < 7 && x >= (1 << left)))
					{
						job->error = __LINE__;
						return VBS_ERR_TOOBIG;
					}
					num |= ((uintmax_t)x) << shift;
				}

				if (num == 0 || num > VBS_DESCRIPTOR_MAX)
				{
					job->error = __LINE__;
					return VBS_ERR_INVALID;
				}

				if ((descriptor & VBS_DESCRIPTOR_MAX) == 0)
					descriptor |= num;
				else
					return VBS_ERR_INVALID;
				goto again;
			}
			else if (!BSET_TEST(&_multi_byte_bset, x))
			{
				job->error = __LINE__;
				return VBS_ERR_INVALID;
			}

			if (num > INTMAX_MAX)
			{
				/* overflow */
				if (!(kind == VBS_INTEGER && negative && (intmax_t)num == INTMAX_MIN))
				{
					job->error = __LINE__;
					return VBS_ERR_TOOBIG;
				}
			}
		}

		job->cur = p;
		job->descriptor = descriptor;
		*p_num = negative ? -(intmax_t)num : num;
		return (vbs_kind_t)kind;
	}

	job->error = __LINE__;
	return VBS_ERR_INCOMPLETE;
}

static inline int _unpack_int(vbs_unpacker_t *job, intmax_t *p_value)
{
	if (VBS_INTEGER != vbs_unpack_kind(job, p_value))
	{
		job->error = __LINE__;
		return -1;
	}

	return 0;
}


inline size_t vbs_size_of_descriptor(int descriptor)
{
	uint32_t value = descriptor;
	if (value && value < (VBS_SPECIAL_DESCRIPTOR | VBS_DESCRIPTOR_MAX))
	{
		size_t n = 0;
		if (value & VBS_SPECIAL_DESCRIPTOR)
		{
			++n;
			value &= VBS_DESCRIPTOR_MAX;
		}

		if (value)
		{
			do {
				++n;
				value >>= 7;
			} while (value > 0x07);
		}
		return n;
	}
	return 0;
}

inline size_t vbs_size_of_integer(intmax_t value)
{
	return value >= 0 ? _intstr_size(value) : _intstr_size(-value);
}

inline size_t vbs_size_of_floating(double value)
{
	intmax_t significand;
	int expo;

	_break_double_value(value, &significand, &expo);
	if (significand < 0)
		significand = -significand;
	return _tag_size(significand) + vbs_size_of_integer(expo);
}

inline size_t vbs_size_of_decimal64(decimal64_t value)
{
	intmax_t significand;
	int expo;

	_break_decimal64_value(value, &significand, &expo);
	if (significand < 0)
		significand = -significand;
	return _tag_size(significand) + vbs_size_of_integer(expo);
}

inline size_t vbs_head_size_of_string(size_t len)
{
	return _intstr_size(len);
}

inline size_t vbs_head_size_of_blob(size_t len)
{
	return _tag_size(len);
}

inline size_t vbs_size_of_string(size_t len)
{
	return vbs_head_size_of_string(len) + len;
}

inline size_t vbs_size_of_blob(size_t len)
{
	return vbs_head_size_of_blob(len) + len;
}

inline size_t vbs_head_size_of_list(int variety)
{
	return variety > 0 ? _tag_size(variety) : 1;
}

inline size_t vbs_head_size_of_dict(int variety)
{
	return variety > 0 ? _tag_size(variety) : 1;
}

inline size_t vbs_buffer_of_descriptor(unsigned char *buf, int descriptor)
{
	uint32_t value = descriptor;
	if (value && value < (VBS_SPECIAL_DESCRIPTOR | VBS_DESCRIPTOR_MAX))
	{
		unsigned char *p = buf;
		if (value & VBS_SPECIAL_DESCRIPTOR)
		{
			*p++ = VBS_DESCRIPTOR;
			value &= VBS_DESCRIPTOR_MAX;
		}

		if (value)
		{
			while (value > 0x07)
			{
				*p++ = 0x80 | value;
				value >>= 7;
			}
			*p++ = VBS_DESCRIPTOR | value;
		}
		return (p - buf);
	}
	return 0;
}

inline size_t vbs_buffer_of_integer(unsigned char *buf, intmax_t value)
{
	return value >= 0 ? _pack_intstr(buf, VBS_INTEGER, value)
			: _pack_intstr(buf, VBS_INTEGER + 0x20, -value);
}

size_t vbs_buffer_of_floating(unsigned char *buf, double value)
{
	intmax_t significand;
	int expo;
	size_t n;

	_break_double_value(value, &significand, &expo);
	if (significand < 0)
		n = _pack_tag(buf, VBS_FLOATING + 1, -significand);
	else
		n = _pack_tag(buf, VBS_FLOATING, significand);

	n += vbs_buffer_of_integer(buf + n, expo);
	return n;
}

size_t vbs_buffer_of_decimal64(unsigned char *buf, decimal64_t value)
{
	intmax_t significand;
	int expo;
	size_t n;

	_break_decimal64_value(value, &significand, &expo);
	if (significand < 0)
		n = _pack_tag(buf, VBS_DECIMAL + 1, -significand);
	else
		n = _pack_tag(buf, VBS_DECIMAL, significand);

	n += vbs_buffer_of_integer(buf + n, expo);
	return n;
}

inline size_t vbs_head_buffer_of_string(unsigned char *buf, size_t strlen)
{
	assert(strlen <= SSIZE_MAX);
	return _pack_intstr(buf, VBS_STRING, strlen);
}

inline size_t vbs_head_buffer_of_blob(unsigned char *buf, size_t bloblen)
{
	assert(bloblen <= SSIZE_MAX);
	return _pack_tag(buf, VBS_BLOB, bloblen);
}

inline size_t vbs_head_buffer_of_list(unsigned char *buf, int variety)
{
	if (variety > 0)
	{
		return _pack_tag(buf, VBS_LIST, variety);
	}

	buf[0] = VBS_LIST;
	return 1;
}

inline size_t vbs_head_buffer_of_dict(unsigned char *buf, int variety)
{
	if (variety > 0)
	{
		return _pack_tag(buf, VBS_DICT, variety);
	}

	buf[0] = VBS_DICT;
	return 1;
}


inline size_t vbs_size_of_data(const vbs_data_t *value)
{
	int n = value->descriptor ? vbs_size_of_descriptor(value->descriptor) : 0;

	if (value->kind == VBS_INTEGER)
		n += vbs_size_of_integer(value->d_int);
	else if (value->kind == VBS_STRING)
		n += vbs_size_of_string(value->d_xstr.len);
	else if (value->kind == VBS_BOOL)
		n += vbs_size_of_bool(value->d_bool);
	else if (value->kind == VBS_FLOATING)
		n += vbs_size_of_floating(value->d_floating);
	else if (value->kind == VBS_DECIMAL)
		n += vbs_size_of_decimal64(value->d_decimal64);
	else if (value->kind == VBS_BLOB)
		n += vbs_size_of_blob(value->d_blob.len);
	else if (value->kind == VBS_NULL)
		n += vbs_size_of_null();
	else if (value->kind == VBS_LIST)
		n += vbs_size_of_list(value->d_list);
	else if (value->kind == VBS_DICT)
		n += vbs_size_of_dict(value->d_dict);

	return n;
}

size_t vbs_size_of_list(const vbs_list_t *vl)
{
	if (vl->_raw.data && vl->_raw.len)
	{
		const xstr_t* raw = &vl->_raw;
		assert(raw->data[raw->len-1] == VBS_TAIL);
		return raw->len;
	}
	else
	{
		size_t n, size = vbs_head_size_of_list(vl->variety) + 1;
		const vbs_litem_t *ent;

		for (ent = vl->first; ent; ent = ent->next)
		{
			if ((n = vbs_size_of_data(&ent->value)) == 0)
				return 0;
			size += n;
		}
		return size;
	}
}

size_t vbs_size_of_dict(const vbs_dict_t *vd)
{
	if (vd->_raw.data && vd->_raw.len)
	{
		const xstr_t* raw = &vd->_raw;
		assert(raw->data[raw->len-1] == VBS_TAIL);
		return raw->len;
	}
	else
	{
		size_t n, size = vbs_head_size_of_dict(vd->variety) + 1;
		const vbs_ditem_t *ent;

		for (ent = vd->first; ent; ent = ent->next)
		{
			if ((n = vbs_size_of_data(&ent->key)) == 0)
				return 0;
			size += n;

			if ((n = vbs_size_of_data(&ent->value)) == 0)
				return 0;
			size += n;
		}
		return size;
	}
}

int vbs_pack_descriptor(vbs_packer_t *job, int descriptor)
{
	if (descriptor)
	{
		unsigned char tmpbuf[TMPBUF_SIZE];
		size_t n = vbs_buffer_of_descriptor(tmpbuf, descriptor);
		if (job->write(job->cookie, tmpbuf, n) != n)
		{
			job->error = __LINE__;
			return -1;
		}
	}
	return 0;
}

int vbs_pack_integer(vbs_packer_t *job, intmax_t value)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_buffer_of_integer(tmpbuf, value);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_uinteger(vbs_packer_t *job, uintmax_t value)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n;

	if ((intmax_t)value < 0)
	{
		job->error = __LINE__;
		return -1;
	}

	n = vbs_buffer_of_integer(tmpbuf, value);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

inline int vbs_pack_lstr(vbs_packer_t *job, const void *str, size_t len)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_head_buffer_of_string(tmpbuf, len);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}

	if (len > 0 && job->write(job->cookie, str, len) != len)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_xstr(vbs_packer_t *job, const xstr_t *str)
{
	return vbs_pack_lstr(job, (const char *)str->data, str->len);
}

int vbs_pack_cstr(vbs_packer_t *job, const char *str)
{
	return vbs_pack_lstr(job, str, strlen(str));
}

int vbs_pack_blob(vbs_packer_t *job, const void *data, size_t len)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_head_buffer_of_blob(tmpbuf, len);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}

	if (len > 0 && job->write(job->cookie, data, len) != len)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_floating(vbs_packer_t *job, double value)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_buffer_of_floating(tmpbuf, value);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_decimal64(vbs_packer_t *job, decimal64_t value)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_buffer_of_decimal64(tmpbuf, value);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_bool(vbs_packer_t *job, bool value)
{
	unsigned char byte = vbs_byte_of_bool(value);
	if (job->write(job->cookie, &byte, 1) != 1)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_null(vbs_packer_t *job)
{
	unsigned char byte = vbs_byte_of_null();
	if (job->write(job->cookie, &byte, 1) != 1)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_head_of_list(vbs_packer_t *job, int variety)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_head_buffer_of_list(tmpbuf, variety);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}

	job->depth++;
	if (job->max_depth >= 0 && job->depth > job->max_depth)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_head_of_dict(vbs_packer_t *job, int variety)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_head_buffer_of_dict(tmpbuf, variety);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}

	job->depth++;
	if (job->max_depth >= 0 && job->depth > job->max_depth)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_tail(vbs_packer_t *job)
{
	unsigned char byte = vbs_byte_of_tail();
	if (job->depth <= 0)
	{
		job->error = __LINE__;
		return -1;
	}

	if (job->write(job->cookie, &byte, 1) != 1)
	{
		job->error = __LINE__;
		return -1;
	}
	job->depth--;
	return 0;
}

inline int vbs_pack_data(vbs_packer_t *job, const vbs_data_t *value)
{
	if (value->descriptor > 0)
	{
		if (vbs_pack_descriptor(job, value->descriptor) < 0)
			return -1;
	}
		
	if (value->kind == VBS_INTEGER)
		return vbs_pack_integer(job, value->d_int);
	else if (value->kind == VBS_STRING)
		return vbs_pack_xstr(job, &value->d_xstr);
	else if (value->kind == VBS_BOOL)
		return vbs_pack_bool(job, value->d_bool);
	else if (value->kind == VBS_FLOATING)
		return vbs_pack_floating(job, value->d_floating);
	else if (value->kind == VBS_DECIMAL)
		return vbs_pack_decimal64(job, value->d_decimal64);
	else if (value->kind == VBS_BLOB)
		return vbs_pack_blob(job, value->d_blob.data, value->d_blob.len);
	else if (value->kind == VBS_NULL)
		return vbs_pack_null(job);
	else if (value->kind == VBS_LIST)
		return vbs_pack_list(job, value->d_list);
	else if (value->kind == VBS_DICT)
		return vbs_pack_dict(job, value->d_dict);
	else if (value->kind == VBS_TAIL)
	{
		job->error = __LINE__;
		return -1;
	}

	job->error = __LINE__;
	return -1;
}

int vbs_pack_list(vbs_packer_t *job, const vbs_list_t *vl)
{
	if (vl->_raw.data && vl->_raw.len)
	{
		const xstr_t *raw = &vl->_raw;
		assert(raw->data[raw->len-1] == VBS_TAIL);

		if (job->write(job->cookie, raw->data, raw->len) != raw->len)
		{
			job->error = __LINE__;
			return -1;
		}
	}
	else
	{
		const vbs_litem_t *ent;

		if (vbs_pack_head_of_list(job, vl->variety) < 0)
			return -1;

		for (ent = vl->first; ent; ent = ent->next)
		{
			if (vbs_pack_data(job, &ent->value) < 0)
				return -1;
		}

		if (vbs_pack_tail(job) < 0)
			return -1;
	}

	return 0;
}

int vbs_pack_dict(vbs_packer_t *job, const vbs_dict_t *vd)
{
	if (vd->_raw.data && vd->_raw.len)
	{
		const xstr_t *raw = &vd->_raw;
		assert(raw->data[raw->len-1] == VBS_TAIL);

		if (job->write(job->cookie, raw->data, raw->len) != raw->len)
		{
			job->error = __LINE__;
			return -1;
		}
	}
	else
	{
		const vbs_ditem_t *ent;

		if (vbs_pack_head_of_dict(job, vd->variety) < 0)
			return -1;

		for (ent = vd->first; ent; ent = ent->next)
		{
			if (vbs_pack_data(job, &ent->key) < 0)
				return -1;
			if (vbs_pack_data(job, &ent->value) < 0)
				return -1;
		}

		if (vbs_pack_tail(job) < 0)
			return -1;
	}

	return 0;
}

int vbs_pack_head_of_string(vbs_packer_t *job, size_t len)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_head_buffer_of_string(tmpbuf, len);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_head_of_blob(vbs_packer_t *job, size_t len)
{
	unsigned char tmpbuf[TMPBUF_SIZE];
	size_t n = vbs_head_buffer_of_blob(tmpbuf, len);
	if (job->write(job->cookie, tmpbuf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_pack_raw(vbs_packer_t *job, const void *buf, size_t n)
{
	if (job->write(job->cookie, buf, n) != n)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}


int vbs_unpack_integer(vbs_unpacker_t *job, intmax_t *p_value)
{
	return _unpack_int(job, p_value);
}

inline int vbs_unpack_lstr(vbs_unpacker_t *job, unsigned char **p_str, ssize_t *p_len)
{
	intmax_t num;

	if (VBS_STRING != vbs_unpack_kind(job, &num))
	{
		job->error = __LINE__;
		return -1;
	}

	if (!_unpacker_size_ok(job, num))
	{
		job->error = __LINE__;
		return -1;
	}

	*p_str = job->cur;
	*p_len = num;
	job->cur += num;
	return 0;
}

int vbs_unpack_xstr(vbs_unpacker_t *job, xstr_t *str)
{
	return vbs_unpack_lstr(job, &str->data, &str->len);
}

int vbs_unpack_blob(vbs_unpacker_t *job, unsigned char **p_data, ssize_t *p_len)
{
	intmax_t num;

	if (VBS_BLOB != vbs_unpack_kind(job, &num))
	{
		job->error = __LINE__;
		return -1;
	}

	if (num && !_unpacker_size_ok(job, num))
	{
		job->error = __LINE__;
		return -1;
	}

	*p_data = job->cur;
	*p_len = num;
	job->cur += num;
	return 0;
}

int vbs_unpack_floating(vbs_unpacker_t *job, double *p_value)
{
	intmax_t significand;
	intmax_t expo;

	if (VBS_FLOATING != vbs_unpack_kind(job, &significand))
	{
		job->error = __LINE__;
		return -1;
	}

	if (_unpack_int(job, &expo) < 0 || job->descriptor > 0)
	{
		job->error = __LINE__;
		return -1;
	}

	if (vbs_make_double_value(p_value, significand, expo) < 0)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_unpack_decimal64(vbs_unpacker_t *job, decimal64_t *p_value)
{
	intmax_t significand;
	intmax_t expo;

	if (VBS_DECIMAL != vbs_unpack_kind(job, &significand))
	{
		job->error = __LINE__;
		return -1;
	}

	if (_unpack_int(job, &expo) < 0 || job->descriptor > 0)
	{
		job->error = __LINE__;
		return -1;
	}

	if (vbs_make_decimal64_value(p_value, significand, expo) < 0)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

int vbs_unpack_bool(vbs_unpacker_t *job, bool *p_value)
{
	if (job->cur >= job->end)
	{
		job->error = __LINE__;
		return -1;
	}

	if ((job->cur[0] & ~0x01) != VBS_BOOL)
	{
		job->error = __LINE__;
		return -1;
	}

	*p_value = (job->cur[0] == (VBS_BOOL + 1));
	job->cur++;
	return 0;
}

int vbs_unpack_null(vbs_unpacker_t *job)
{
	intmax_t num;
	if (vbs_unpack_kind(job, &num) != VBS_NULL)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

static inline int _unpack_verify_kind(vbs_unpacker_t *job, int kind, int *variety)
{
	intmax_t num;

	if (kind != vbs_unpack_kind(job, &num) || num < 0 || num > INT_MAX)
	{
		job->error = __LINE__;
		return -1;
	}

	if (variety)
		*variety = num;
	return 0;
}

inline int vbs_unpack_head_of_list(vbs_unpacker_t *job, int *variety)
{
	if (_unpack_verify_kind(job, VBS_LIST, variety) < 0)
		return -1;

	job->depth++;
	if (job->max_depth >= 0 && job->depth > job->max_depth)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

inline int vbs_unpack_head_of_dict(vbs_unpacker_t *job, int *variety)
{
	if (_unpack_verify_kind(job, VBS_DICT, variety) < 0)
		return -1;

	job->depth++;
	if (job->max_depth >= 0 && job->depth > job->max_depth)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

static inline int _unpack_verify_simple_kind(vbs_unpacker_t *job, int kind)
{
	if (job->cur >= job->end || job->cur[0] != kind)
	{
		job->error = __LINE__;
		return -1;
	}

	job->descriptor = 0;
	job->cur++;
	return 0;
}

int vbs_unpack_tail(vbs_unpacker_t *job)
{
	if (job->depth <= 0)
	{
		job->error = __LINE__;
		return -1;
	}

	if (_unpack_verify_simple_kind(job, VBS_TAIL) < 0)
		return -1;

	job->depth--;
	return 0;
}

int vbs_unpack_primitive(vbs_unpacker_t *job, vbs_data_t *pv, int *variety/*NULL*/)
{
	intmax_t num;
	intmax_t expo;

	vbs_data_init(pv);
	pv->kind = vbs_unpack_kind(job, &num);
	pv->descriptor = job->descriptor;
	switch (pv->kind)
	{
	case VBS_INTEGER:
		pv->d_int = num;
		break;

	case VBS_STRING:
	case VBS_BLOB:
		if (num && !_unpacker_size_ok(job, num))
		{
			job->error = __LINE__;
			goto error;
		}
		pv->d_xstr.data = job->cur;
		pv->d_xstr.len = num;
		job->cur += num;
		break;

	case VBS_BOOL:
		pv->d_bool = num;
		break;

	case VBS_LIST:
	case VBS_DICT:
		job->depth++;
		if (job->max_depth >= 0 && job->depth > job->max_depth)
		{
			job->error = __LINE__;
			goto error;
		}

		if (num < 0 || num > INT_MAX)
		{
			job->error = __LINE__;
			goto error;
		}

		if (variety)
			*variety = num;
		break;

	case VBS_TAIL:
		job->depth--;
		if (job->depth < 0)
		{
			job->error = __LINE__;
			goto error;
		}
		break;

	case VBS_FLOATING:
		if (_unpack_int(job, &expo) < 0 || job->descriptor > 0)
		{
			job->error = __LINE__;
			goto error;
		}

		if (vbs_make_double_value(&pv->d_floating, num, expo) < 0)
		{
			job->error = __LINE__;
			goto error;
		}
		break;

	case VBS_DECIMAL:
		if (_unpack_int(job, &expo) < 0 || job->descriptor > 0)
		{
			job->error = __LINE__;
			goto error;
		}

		if (vbs_make_decimal64_value(&pv->d_decimal64, num, expo) < 0)
		{
			job->error = __LINE__;
			goto error;
		}
			
		break;

	case VBS_NULL:
		/* Do nothing */
		break;

	default:
		job->error = __LINE__;
		goto error;
	}

	return pv->kind;
error:
	pv->kind = VBS_ERR_INVALID;
	return -1;
}


static int _skip_to_tail(vbs_unpacker_t *job, bool dict);

inline int vbs_skip_body_of_list(vbs_unpacker_t *job)
{
	return _skip_to_tail(job, false);
}

inline int vbs_skip_body_of_dict(vbs_unpacker_t *job)
{
	return _skip_to_tail(job, true);
}

static int _skip_primitive(vbs_unpacker_t *job)
{
	intmax_t num;
	intmax_t expo;
	vbs_kind_t kind = vbs_unpack_kind(job, &num);

	switch (kind)
	{
	case VBS_STRING:
	case VBS_BLOB:
		if (num && !_unpacker_size_ok(job, num))
		{
			job->error = __LINE__;
			goto error;
		}
		job->cur += num;
		break;

	case VBS_LIST:
	case VBS_DICT:
		job->depth++;
		if (job->max_depth >= 0 && job->depth > job->max_depth)
		{
			job->error = __LINE__;
			goto error;
		}
		if (num < 0 || num > INT_MAX)
		{
			job->error = __LINE__;
			goto error;
		}
		break;

	case VBS_TAIL:
		job->depth--;
		if (job->depth < 0)
		{
			job->error = __LINE__;
			goto error;
		}
		break;

	case VBS_FLOATING:
	case VBS_DECIMAL:
		if (_unpack_int(job, &expo) < 0 || job->descriptor > 0)
		{
			job->error = __LINE__;
			goto error;
		}
		break;

	case VBS_INTEGER:
	case VBS_BOOL:
	case VBS_NULL:
		/* Do nothing */
		break;

	default:
		job->error = __LINE__;
		goto error;
	}

	return kind;
error:
	return -1;
}

int vbs_unpack_raw(vbs_unpacker_t *job, unsigned char **pbuf, ssize_t *plen)
{
	unsigned char *old_cur = job->cur;
	int kind = _skip_primitive(job);

	if (kind == VBS_LIST)
	{
		if (vbs_skip_body_of_list(job) < 0)
			return -1;
	}
	else if (kind == VBS_DICT)
	{
		if (vbs_skip_body_of_dict(job) < 0)
			return -1;
	}

	if (pbuf)
		*pbuf = old_cur;

	if (plen)
		*plen = (job->cur - old_cur);

	return kind;
}

static inline int _skip_data(vbs_unpacker_t *job)
{
	vbs_data_t val;
	if (vbs_unpack_primitive(job, &val, NULL) < 0)
		return -1;

	if (val.kind == VBS_LIST)
		return vbs_skip_body_of_list(job);
	else if (val.kind == VBS_DICT)
		return vbs_skip_body_of_dict(job);
	else if (val.kind == VBS_TAIL)
		return -1;

	return 0;
}

static int _skip_to_tail(vbs_unpacker_t *job, bool dict)
{
	size_t n;
	for (n = 0; true; ++n)
	{
		if (vbs_unpack_if_tail(job))
		{
			if (dict && (n % 2) != 0)
				return -1;
			return 0;
		}

		if (_skip_data(job) < 0)
			return -1;
	}

	return -1;
}

vbs_kind_t vbs_peek_kind(vbs_unpacker_t *job)
{
	intmax_t num;
	vbs_kind_t kind;
	unsigned char *cur = job->cur;

	kind = vbs_unpack_kind(job, &num);
	job->cur = cur;
	return kind;
}

bool (vbs_unpack_if_tail)(vbs_unpacker_t *job)
{
	/* NB: call the macro with the same name */
	return vbs_unpack_if_tail(job);
}


static int _unpack_check_int(vbs_unpacker_t *job, intmax_t *p_value, intmax_t min, intmax_t max)
{
	if (_unpack_int(job, p_value) < 0 || *p_value < min || *p_value > max)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

#define UNPACK_INT(JOB, PVAL, TMP, MIN, MAX)		\
	(_unpack_check_int((JOB), &(TMP), (MIN), (MAX)) == 0 ? (*(PVAL) = (TMP), 0) : -1)

int vbs_unpack_int8(vbs_unpacker_t *job, int8_t *p_value)
{
	intmax_t x;
	return UNPACK_INT(job, p_value, x, INT8_MIN, INT8_MAX);
}

int vbs_unpack_int16(vbs_unpacker_t *job, int16_t *p_value)
{
	intmax_t x;
	return UNPACK_INT(job, p_value, x, INT16_MIN, INT16_MAX);
}

int vbs_unpack_int32(vbs_unpacker_t *job, int32_t *p_value)
{
	intmax_t x;
	return UNPACK_INT(job, p_value, x, INT32_MIN, INT32_MAX);
}

int vbs_unpack_int64(vbs_unpacker_t *job, int64_t *p_value)
{
	intmax_t x;
	return UNPACK_INT(job, p_value, x, INT64_MIN, INT64_MAX);
}


static int _unpack_check_uint(vbs_unpacker_t *job, uintmax_t *p_value, uintmax_t max)
{
	if (vbs_unpack_kind(job, (intmax_t *)p_value) != VBS_INTEGER || (intmax_t)*p_value < 0 || *p_value > max)
	{
		job->error = __LINE__;
		return -1;
	}
	return 0;
}

#define UNAPCK_UINT(JOB, PVAL, TMP, MAX)		\
	(_unpack_check_uint((JOB), &(TMP), (MAX)) == 0 ? (*(PVAL) = (TMP), 0) : -1)

int vbs_unpack_uint8(vbs_unpacker_t *job, uint8_t *p_value)
{
	uintmax_t x;
	return UNAPCK_UINT(job, p_value, x, UINT8_MAX);
}

int vbs_unpack_uint16(vbs_unpacker_t *job, uint16_t *p_value)
{
	uintmax_t x;
	return UNAPCK_UINT(job, p_value, x, UINT16_MAX);
}

int vbs_unpack_uint32(vbs_unpacker_t *job, uint32_t *p_value)
{
	uintmax_t x;
	return UNAPCK_UINT(job, p_value, x, UINT32_MAX);
}

int vbs_unpack_uint64(vbs_unpacker_t *job, uint64_t *p_value)
{
	uintmax_t x;
	return UNAPCK_UINT(job, p_value, x, UINT64_MAX);
}

static int _unpack_body_of_list(vbs_unpacker_t *job, vbs_list_t *list, const xmem_t *xm, void *xm_cookie);
static int _unpack_body_of_dict(vbs_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie);

static inline int _unpack_data(vbs_unpacker_t *job, vbs_data_t *pv, const xmem_t *xm, void *xm_cookie)
{
	int variety;
	if (vbs_unpack_primitive(job, pv, &variety) < 0)
		return -1;

	if (pv->kind == VBS_LIST)
	{
		pv->d_list = (vbs_list_t *)xm->alloc(xm_cookie, sizeof(*pv->d_list));
		vbs_list_init(pv->d_list, variety);
		return _unpack_body_of_list(job, pv->d_list, xm, xm_cookie);
	}
	else if (pv->kind == VBS_DICT)
	{
		pv->d_dict = (vbs_dict_t *)xm->alloc(xm_cookie, sizeof(*pv->d_dict));
		vbs_dict_init(pv->d_dict, variety);
		return _unpack_body_of_dict(job, pv->d_dict, xm, xm_cookie);
	}
	else if (pv->kind == VBS_TAIL)
	{
		job->error = __LINE__;
		return -1;
	}

	return 0;
}

static int _unpack_body_of_list(vbs_unpacker_t *job, vbs_list_t *list, const xmem_t *xm, void *xm_cookie)
{
	int rc = -1;
	uint8_t *begin = job->cur - _tag_size(list->variety);

	while (true)
	{
		vbs_litem_t *ent;
		if (vbs_unpack_if_tail(job))
		{
			list->_raw.data = begin;
			list->_raw.len = job->cur - begin;
			break;
		}

		ent = (vbs_litem_t *)xm->alloc(xm_cookie, sizeof(*ent));
		if (!ent)
		{
			job->error = __LINE__;
			goto error;
		}

		vbs_litem_init(ent);
		vbs_list_push_back(list, ent);
		if (_unpack_data(job, &ent->value, xm, xm_cookie) < 0)
			goto error;
	}
	rc = 0;
error:
	return rc;
}

int vbs_unpack_body_of_list(vbs_unpacker_t *job, vbs_list_t *list, const xmem_t *xm, void *xm_cookie)
{
	return _unpack_body_of_list(job, list, xm ? xm : &stdc_xmem, xm_cookie);
}

int _unpack_body_of_dict(vbs_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie)
{
	uint8_t *begin = job->cur - _tag_size(dict->variety);
	int rc = -1;

	while (true)
	{
		vbs_ditem_t *ent;
		if (vbs_unpack_if_tail(job))
		{
			dict->_raw.data = begin;
			dict->_raw.len = job->cur - begin;
			break;
		}

		ent = (vbs_ditem_t *)xm->alloc(xm_cookie, sizeof(*ent));
		if (!ent)
		{
			job->error = __LINE__;
			goto error;
		}

		vbs_ditem_init(ent);
		vbs_dict_push_back(dict, ent);
		if (_unpack_data(job, &ent->key, xm, xm_cookie) < 0)
			goto error;
		if (_unpack_data(job, &ent->value, xm, xm_cookie) < 0)
			goto error;
	}
	rc = 0;
error:
	return rc;
}

int vbs_unpack_body_of_dict(vbs_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie)
{
	return _unpack_body_of_dict(job, dict, xm ? xm : &stdc_xmem, xm_cookie);
}

int vbs_unpack_list(vbs_unpacker_t *job, vbs_list_t *list, const xmem_t *xm, void *xm_cookie)
{
	int variety;

	if (vbs_unpack_head_of_list(job, &variety) < 0)
		return -1;

	if (!xm)
		xm = &stdc_xmem;

	vbs_list_init(list, variety);
	if (_unpack_body_of_list(job, list, xm, xm_cookie) < 0)
	{
		vbs_release_list(list, xm, xm_cookie);
		return -1;
	}
	return 0;
}

int vbs_unpack_dict(vbs_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie)
{
	int variety;

	if (vbs_unpack_head_of_dict(job, &variety) < 0)
		return -1;

	if (!xm)
		xm = &stdc_xmem;

	vbs_dict_init(dict, variety);
	if (_unpack_body_of_dict(job, dict, xm, xm_cookie) < 0)
	{
		vbs_release_dict(dict, xm, xm_cookie);
		return -1;
	}
	return 0;
}

int vbs_unpack_data(vbs_unpacker_t *job, vbs_data_t *pv, const xmem_t *xm, void *xm_cookie)
{
	if (!xm)
		xm = &stdc_xmem;

	if (_unpack_data(job, pv, xm, xm_cookie) < 0)
	{
		vbs_release_data(pv, xm, xm_cookie);
		return -1;
	}
	return 0;
}


static void _release_list(vbs_list_t *list, bool release_self, const xmem_t *xm, void *xm_cookie);
static void _release_dict(vbs_dict_t *dict, bool release_self, const xmem_t *xm, void *xm_cookie);

static inline void _release_data(vbs_data_t *pv, const xmem_t *xm, void *xm_cookie)
{
	if (pv->kind == VBS_LIST)
	{
		_release_list(pv->d_list, true, xm, xm_cookie);
	}
	else if (pv->kind == VBS_DICT)
	{
		_release_dict(pv->d_dict, true, xm, xm_cookie);
	}
	else if (pv->kind == VBS_STRING && pv->is_owner)
	{
		xm->free(xm_cookie, pv->d_xstr.data);
	}
	else if (pv->kind == VBS_BLOB && pv->is_owner)
	{
		xm->free(xm_cookie, pv->d_blob.data);
	}
}

static void _release_list(vbs_list_t *list, bool release_self, const xmem_t *xm, void *xm_cookie)
{
	vbs_litem_t *ent, *next;
	for (ent = list->first; ent; ent = next)
	{
		next = ent->next;

		_release_data(&ent->value, xm, xm_cookie);
		xm->free(xm_cookie, ent);
	}

	if (release_self)
		xm->free(xm_cookie, list);
}

static void _release_dict(vbs_dict_t *dict, bool release_self, const xmem_t *xm, void *xm_cookie)
{
	vbs_ditem_t *ent, *next;
	for (ent = dict->first; ent; ent = next)
	{
		next = ent->next;

		_release_data(&ent->key, xm, xm_cookie);
		_release_data(&ent->value, xm, xm_cookie);
		xm->free(xm_cookie, ent);
	}

	if (release_self)
		xm->free(xm_cookie, dict);
}

void vbs_release_list(vbs_list_t *list, const xmem_t *xm, void *xm_cookie)
{
	if (list->count && (!xm || xm->free))
	{
		_release_list(list, false, xm ? xm : &stdc_xmem, xm_cookie);
	}
	memset(list, 0, sizeof(*list));
}

void vbs_release_dict(vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie)
{
	if (dict->count && (!xm || xm->free))
	{
		_release_dict(dict, false, xm ? xm : &stdc_xmem, xm_cookie);
	}
	memset(dict, 0, sizeof(*dict));
}

void vbs_release_data(vbs_data_t *pv, const xmem_t *xm, void *xm_cookie)
{
	if (!xm || xm->free)
	{
		_release_data(pv, xm ? xm : &stdc_xmem, xm_cookie);
	}
	memset(pv, 0, sizeof(*pv));
}


vbs_data_t *vbs_dict_get_data(const vbs_dict_t *dict, const char *key)
{
        vbs_ditem_t *ent;
        for (ent = dict->first; ent; ent = ent->next)
        {
                if (ent->key.kind != VBS_STRING)
                        continue;

                if (xstr_equal_cstr(&ent->key.d_xstr, key))
                        return &ent->value;
        }
        return NULL;
}

intmax_t vbs_dict_get_integer(const vbs_dict_t *d, const char *key, intmax_t dft)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	return (v && v->kind == VBS_INTEGER) ? v->d_int : dft;
}

bool vbs_dict_get_bool(const vbs_dict_t *d, const char *key, bool dft)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	return (v && v->kind == VBS_BOOL) ? v->d_bool : dft;
}

double vbs_dict_get_floating(const vbs_dict_t *d, const char *key, double dft)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	if (v)
	{
		if (v->kind == VBS_FLOATING)
			return v->d_floating;
		else if (v->kind == VBS_INTEGER)
			return v->d_int;
	}
	return dft;
}

decimal64_t vbs_dict_get_decimal64(const vbs_dict_t *d, const char *key, decimal64_t dft)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	return (v && v->kind == VBS_DECIMAL) ? v->d_decimal64 : dft;
}

xstr_t vbs_dict_get_xstr(const vbs_dict_t *d, const char *key)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	return (v && v->kind == VBS_STRING) ? v->d_xstr : xstr_null;
}

xstr_t vbs_dict_get_blob(const vbs_dict_t *d, const char *key)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	if (v)
	{
		if (v->kind == VBS_BLOB)
			return v->d_blob;
		else if (v->kind == VBS_STRING)
			return v->d_xstr;
	}
	return xstr_null;
}

vbs_list_t *vbs_dict_get_list(const vbs_dict_t *d, const char *key)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	return (v && v->kind == VBS_LIST) ? v->d_list : NULL;
}

vbs_dict_t *vbs_dict_get_dict(const vbs_dict_t *d, const char *key)
{
	vbs_data_t *v = vbs_dict_get_data(d, key);
	return (v && v->kind == VBS_DICT) ? v->d_dict : NULL;
}

decimal64_t vbs_data_get_decimal64(const vbs_data_t *v, decimal64_t dft)
{
	if (v->kind == VBS_DECIMAL)
		return v->d_decimal64;
	else if (v->kind == VBS_INTEGER)
	{
		decimal64_t d;
		if (decimal64_from_integer(&d, v->d_int) == 0)
			return d;
	}
	return dft;
}

