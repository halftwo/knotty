/*
   The implementation is modified from glibc.
 */
#define _XOPEN_SOURCE 600
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "xformat.h"
#include "floating.h"
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xformat.c,v 1.43 2014/01/26 09:53:22 gremlin Exp $";
#endif

#if ((defined(__linux) || defined(__FreeBSD__)) && (defined(__i386) || defined(__x86_64)))
#define long_double_union		ieee854_binary80
#define LONG_DOUBLE_BIAS		IEEE854_BINARY80_BIAS
#define LONG_DOUBLE_MANTISSA_BITS	63
#else
#define long_double_union		ieee754_binary64
#define LONG_DOUBLE_BIAS		IEEE754_BINARY64_BIAS
#define LONG_DOUBLE_MANTISSA_BITS	52
#endif


#define REF(x)	do_##x


enum
{
	REF (form_unknown),
	REF (flag_space),         /* for ' ' */
	REF (flag_plus),          /* for '+' */
	REF (flag_minus),         /* for '-' */
	REF (flag_hash),          /* for '#' */
	REF (flag_zero),          /* for '0' */
	REF (flag_quote),         /* for '\'' */
	REF (width_asterics),     /* for '*' */
	REF (width),              /* for '1'...'9' */
	REF (precision),          /* for '.' */
	REF (mod_half),           /* for 'h' */
	REF (mod_long),           /* for 'l' */
	REF (mod_longlong),       /* for 'L', 'q' */
	REF (mod_size_t),         /* for 'z', 'Z' */
	REF (form_percent),       /* for '%' */
	REF (form_integer),       /* for 'd', 'i' */
	REF (form_unsigned),      /* for 'u' */
	REF (form_octal),         /* for 'o' */
	REF (form_hexa),          /* for 'X', 'x' */
	REF (form_float),         /* for 'E', 'e', 'F', 'f', 'G', 'g' */
	REF (form_character),     /* for 'c' */
	REF (form_string),        /* for 's', 'S' */
	REF (form_pointer),       /* for 'p' */
	REF (form_number),        /* for 'n' */
	REF (form_strerror),      /* for 'm' */
	REF (form_wcharacter),    /* for 'C' */
	REF (form_floathex),      /* for 'A', 'a' */
	REF (mod_ptrdiff_t),      /* for 't' */
	REF (mod_intmax_t),       /* for 'j' */
	REF (flag_i18n),          /* for 'I' */

	N_JUMP_TABLE,

	REF (mod_halfhalf),
};


static const unsigned char char_kinds[0x80 - ' '] =
{
    /*   */  1,          0,          0, /* # */  4,
	     0, /* % */ 14,          0, /* ' */  6,
	     0,          0, /* * */  7, /* + */  2,
	     0, /* - */  3, /* . */  9,          0,
    /* 0 */  5, /* 1 */  8, /* 2 */  8, /* 3 */  8,
    /* 4 */  8, /* 5 */  8, /* 6 */  8, /* 7 */  8,
    /* 8 */  8, /* 9 */  8,          0,          0,
	     0,          0,          0,          0,
	     0, /* A */ 26,          0, /* C */ 25,
	     0, /* E */ 19, /* F */ 19, /* G */ 19,
	     0, /* I */ 29,          0,          0,
    /* L */ 12,          0,          0,          0,
	     0,          0,          0, /* S */ 21,
	     0,          0,          0,          0,
    /* X */ 18,          0, /* Z */ 13,          0,
	     0,          0,          0,          0,
	     0, /* a */ 26,          0, /* c */ 20,
    /* d */ 15, /* e */ 19, /* f */ 19, /* g */ 19,
    /* h */ 10, /* i */ 15, /* j */ 28,          0,
    /* l */ 11, /* m */ 24, /* n */ 23, /* o */ 17,
    /* p */ 22, /* q */ 12,          0, /* s */ 21,
    /* t */ 27, /* u */ 16,          0,          0,
    /* x */ 18,          0, /* z */ 13,          0,
	     0,          0,          0,	         0,
};

typedef unsigned char JUMP_TABLE_TYPE;

/* Step 0: at the beginning.  */
static JUMP_TABLE_TYPE step0_jumps[N_JUMP_TABLE] =
{
	REF (form_unknown),
	REF (flag_space),         /* for ' ' */
	REF (flag_plus),          /* for '+' */
	REF (flag_minus),         /* for '-' */
	REF (flag_hash),          /* for '#' */
	REF (flag_zero),          /* for '0' */
	REF (flag_quote),         /* for '\'' */
	REF (width_asterics),     /* for '*' */
	REF (width),              /* for '1'...'9' */
	REF (precision),          /* for '.' */
	REF (mod_half),           /* for 'h' */
	REF (mod_long),           /* for 'l' */
	REF (mod_longlong),       /* for 'L', 'q' */
	REF (mod_size_t),         /* for 'z', 'Z' */
	REF (form_percent),       /* for '%' */
	REF (form_integer),       /* for 'd', 'i' */
	REF (form_unsigned),      /* for 'u' */
	REF (form_octal),         /* for 'o' */
	REF (form_hexa),          /* for 'X', 'x' */
	REF (form_float),         /* for 'E', 'e', 'F', 'f', 'G', 'g' */
	REF (form_character),     /* for 'c' */
	REF (form_string),        /* for 's', 'S' */
	REF (form_pointer),       /* for 'p' */
	REF (form_number),        /* for 'n' */
	REF (form_strerror),      /* for 'm' */
	REF (form_wcharacter),    /* for 'C' */
	REF (form_floathex),      /* for 'A', 'a' */
	REF (mod_ptrdiff_t),      /* for 't' */
	REF (mod_intmax_t),       /* for 'j' */
	REF (flag_i18n),          /* for 'I' */
};

/* Step 1: after processing width.  */
static JUMP_TABLE_TYPE step1_jumps[N_JUMP_TABLE] =
{
	REF (form_unknown),
	REF (form_unknown),       /* for ' ' */
	REF (form_unknown),       /* for '+' */
	REF (form_unknown),       /* for '-' */
	REF (form_unknown),       /* for '#' */
	REF (form_unknown),       /* for '0' */
	REF (form_unknown),       /* for '\'' */
	REF (form_unknown),       /* for '*' */
	REF (form_unknown),       /* for '1'...'9' */
	REF (precision),          /* for '.' */
	REF (mod_half),           /* for 'h' */
	REF (mod_long),           /* for 'l' */
	REF (mod_longlong),       /* for 'L', 'q' */
	REF (mod_size_t),         /* for 'z', 'Z' */
	REF (form_percent),       /* for '%' */
	REF (form_integer),       /* for 'd', 'i' */
	REF (form_unsigned),      /* for 'u' */
	REF (form_octal),         /* for 'o' */
	REF (form_hexa),          /* for 'X', 'x' */
	REF (form_float),         /* for 'E', 'e', 'F', 'f', 'G', 'g' */
	REF (form_character),     /* for 'c' */
	REF (form_string),        /* for 's', 'S' */
	REF (form_pointer),       /* for 'p' */
	REF (form_number),        /* for 'n' */
	REF (form_strerror),      /* for 'm' */
	REF (form_wcharacter),    /* for 'C' */
	REF (form_floathex),      /* for 'A', 'a' */
	REF (mod_ptrdiff_t),      /* for 't' */
	REF (mod_intmax_t),       /* for 'j' */
	REF (form_unknown),       /* for 'I' */
};

/* Step 2: after processing precision.  */
static JUMP_TABLE_TYPE step2_jumps[N_JUMP_TABLE] =
{
	REF (form_unknown),
	REF (form_unknown),       /* for ' ' */
	REF (form_unknown),       /* for '+' */
	REF (form_unknown),       /* for '-' */
	REF (form_unknown),       /* for '#' */
	REF (form_unknown),       /* for '0' */
	REF (form_unknown),       /* for '\'' */
	REF (form_unknown),       /* for '*' */
	REF (form_unknown),       /* for '1'...'9' */
	REF (form_unknown),       /* for '.' */
	REF (mod_half),           /* for 'h' */
	REF (mod_long),           /* for 'l' */
	REF (mod_longlong),       /* for 'L', 'q' */
	REF (mod_size_t),         /* for 'z', 'Z' */
	REF (form_percent),       /* for '%' */
	REF (form_integer),       /* for 'd', 'i' */
	REF (form_unsigned),      /* for 'u' */
	REF (form_octal),         /* for 'o' */
	REF (form_hexa),          /* for 'X', 'x' */
	REF (form_float),         /* for 'E', 'e', 'F', 'f', 'G', 'g' */
	REF (form_character),     /* for 'c' */
	REF (form_string),        /* for 's', 'S' */
	REF (form_pointer),       /* for 'p' */
	REF (form_number),        /* for 'n' */
	REF (form_strerror),      /* for 'm' */
	REF (form_wcharacter),    /* for 'C' */
	REF (form_floathex),      /* for 'A', 'a' */
	REF (mod_ptrdiff_t),      /* for 't' */
	REF (mod_intmax_t),       /* for 'j' */
	REF (form_unknown),       /* for 'I' */
};

/* Step 3a: after processing first 'h' modifier.  */
static JUMP_TABLE_TYPE step3a_jumps[N_JUMP_TABLE] =
{
	REF (form_unknown),
	REF (form_unknown),       /* for ' ' */
	REF (form_unknown),       /* for '+' */
	REF (form_unknown),       /* for '-' */
	REF (form_unknown),       /* for '#' */
	REF (form_unknown),       /* for '0' */
	REF (form_unknown),       /* for '\'' */
	REF (form_unknown),       /* for '*' */
	REF (form_unknown),       /* for '1'...'9' */
	REF (form_unknown),       /* for '.' */
	REF (mod_halfhalf),       /* for 'h' */
	REF (form_unknown),       /* for 'l' */
	REF (form_unknown),       /* for 'L', 'q' */
	REF (form_unknown),       /* for 'z', 'Z' */
	REF (form_percent),       /* for '%' */
	REF (form_integer),       /* for 'd', 'i' */
	REF (form_unsigned),      /* for 'u' */
	REF (form_octal),         /* for 'o' */
	REF (form_hexa),          /* for 'X', 'x' */
	REF (form_unknown),       /* for 'E', 'e', 'F', 'f', 'G', 'g' */
	REF (form_unknown),       /* for 'c' */
	REF (form_unknown),       /* for 's', 'S' */
	REF (form_unknown),       /* for 'p' */
	REF (form_number),        /* for 'n' */
	REF (form_unknown),       /* for 'm' */
	REF (form_unknown),       /* for 'C' */
	REF (form_unknown),       /* for 'A', 'a' */
	REF (form_unknown),       /* for 't' */
	REF (form_unknown),       /* for 'j' */
	REF (form_unknown),       /* for 'I' */
};

/* Step 3b: after processing first 'l' modifier.  */
static JUMP_TABLE_TYPE step3b_jumps[N_JUMP_TABLE] =
{
	REF (form_unknown),
	REF (form_unknown),       /* for ' ' */
	REF (form_unknown),       /* for '+' */
	REF (form_unknown),       /* for '-' */
	REF (form_unknown),       /* for '#' */
	REF (form_unknown),       /* for '0' */
	REF (form_unknown),       /* for '\'' */
	REF (form_unknown),       /* for '*' */
	REF (form_unknown),       /* for '1'...'9' */
	REF (form_unknown),       /* for '.' */
	REF (form_unknown),       /* for 'h' */
	REF (mod_longlong),       /* for 'l' */
	REF (form_unknown),       /* for 'L', 'q' */
	REF (form_unknown),       /* for 'z', 'Z' */
	REF (form_percent),       /* for '%' */
	REF (form_integer),       /* for 'd', 'i' */
	REF (form_unsigned),      /* for 'u' */
	REF (form_octal),         /* for 'o' */
	REF (form_hexa),          /* for 'X', 'x' */
	REF (form_float),         /* for 'E', 'e', 'F', 'f', 'G', 'g' */
	REF (form_character),     /* for 'c' */
	REF (form_string),        /* for 's', 'S' */
	REF (form_pointer),       /* for 'p' */
	REF (form_number),        /* for 'n' */
	REF (form_strerror),      /* for 'm' */
	REF (form_wcharacter),    /* for 'C' */
	REF (form_floathex),      /* for 'A', 'a' */
	REF (form_unknown),       /* for 't' */
	REF (form_unknown),       /* for 'j' */
	REF (form_unknown),       /* for 'I' */
};

/* Step 4: processing format specifier.  */
static JUMP_TABLE_TYPE step4_jumps[N_JUMP_TABLE] =
{
	REF (form_unknown),
	REF (form_unknown),       /* for ' ' */
	REF (form_unknown),       /* for '+' */
	REF (form_unknown),       /* for '-' */
	REF (form_unknown),       /* for '#' */
	REF (form_unknown),       /* for '0' */
	REF (form_unknown),       /* for '\'' */
	REF (form_unknown),       /* for '*' */
	REF (form_unknown),       /* for '1'...'9' */
	REF (form_unknown),       /* for '.' */
	REF (form_unknown),       /* for 'h' */
	REF (form_unknown),       /* for 'l' */
	REF (form_unknown),       /* for 'L', 'q' */
	REF (form_unknown),       /* for 'z', 'Z' */
	REF (form_percent),       /* for '%' */
	REF (form_integer),       /* for 'd', 'i' */
	REF (form_unsigned),      /* for 'u' */
	REF (form_octal),         /* for 'o' */
	REF (form_hexa),          /* for 'X', 'x' */
	REF (form_float),         /* for 'E', 'e', 'F', 'f', 'G', 'g' */
	REF (form_character),     /* for 'c' */
	REF (form_string),        /* for 's', 'S' */
	REF (form_pointer),       /* for 'p' */
	REF (form_number),        /* for 'n' */
	REF (form_strerror),      /* for 'm' */
	REF (form_wcharacter),    /* for 'C' */
	REF (form_floathex),      /* for 'A', 'a' */
	REF (form_unknown),       /* for 't' */
	REF (form_unknown),       /* for 'j' */
	REF (form_unknown),       /* for 'I' */
};

#define WHAT_TODO(ch, jmptab)  (((signed char)ch < ' ') ? 0 : jmptab[char_kinds[ch - ' ']])


struct out_struct
{
	ssize_t count;
	xio_write_function write;
	void *cookie;
};

static ssize_t out_write(struct out_struct *os, const char *data, size_t size)
{
	ssize_t rc = os->write(os->cookie, data, size);
	if (rc > 0)
		os->count += rc;
	if (rc != (ssize_t)size)
		os->count = -1;		/* Indicate an error */
	return rc;
}

static xio_t out_xio =
{
	NULL,
	(xio_write_function)out_write,
};

struct buf_struct
{
	ssize_t count;
	char *buf;
	ssize_t *used;
	ssize_t size;
	bool no_extra;
};

static ssize_t buf_write(struct buf_struct *bs, const char *data, size_t size)
{
	ssize_t used = *bs->used;
	size_t n = 0;
	if (used < bs->size)
	{
		n = bs->size - used;
		if (n > size)
			n = size;

		if (n > 24)
		{
			memcpy(bs->buf + used, data, n);
		}
		else if (n > 0)
		{
			switch (n)
			{
			case 24: bs->buf[used++] = *(char *)data++;
			case 23: bs->buf[used++] = *(char *)data++;
			case 22: bs->buf[used++] = *(char *)data++;
			case 21: bs->buf[used++] = *(char *)data++;
			case 20: bs->buf[used++] = *(char *)data++;
			case 19: bs->buf[used++] = *(char *)data++;
			case 18: bs->buf[used++] = *(char *)data++;
			case 17: bs->buf[used++] = *(char *)data++;
			case 16: bs->buf[used++] = *(char *)data++;
			case 15: bs->buf[used++] = *(char *)data++;
			case 14: bs->buf[used++] = *(char *)data++;
			case 13: bs->buf[used++] = *(char *)data++;
			case 12: bs->buf[used++] = *(char *)data++;
			case 11: bs->buf[used++] = *(char *)data++;
			case 10: bs->buf[used++] = *(char *)data++;
			case 9: bs->buf[used++] = *(char *)data++;
			case 8: bs->buf[used++] = *(char *)data++;
			case 7: bs->buf[used++] = *(char *)data++;
			case 6: bs->buf[used++] = *(char *)data++;
			case 5: bs->buf[used++] = *(char *)data++;
			case 4: bs->buf[used++] = *(char *)data++;
			case 3: bs->buf[used++] = *(char *)data++;
			case 2: bs->buf[used++] = *(char *)data++;
			case 1: bs->buf[used++] = *(char *)data++;
			}
		}
		*bs->used += n;
	}
	bs->count += size;
	return bs->no_extra ? n : size;
}

static xio_t buf_xio = {
	NULL,
	(xio_write_function)buf_write,
};


static long double _dtab16[] = {
	1e-000L, 1e-016L, 1e-032L, 1e-048L, 1e-064L, 1e-080L, 1e-096L, 1e-112L,
	1e-128L, 1e-144L, 1e-160L, 1e-176L, 1e-192L, 1e-208L, 1e-224L, 1e-240L,
	1e-256L, 1e-272L, 1e-288L, 1e-304L, 1e-320L,
};

static long double _dtab1[] = {
	1e-000L, 1e-001L, 1e-002L, 1e-003L, 1e-004L, 1e-005L, 1e-006L, 1e-007L,
	1e-008L, 1e-009L, 1e-010L, 1e-011L, 1e-012L, 1e-013L, 1e-014L, 1e-015L,
};

static long double _adjust_ld(long double value, int n)
{
	int q;

	if (n > 0)
	{
		value *= _dtab1[n % 16]; 
		q = n / 16;
		if (q >= 20)
		{
			do
			{
				value *= _dtab16[20];
				q -= 20;
			} while (q >= 20);
		}
		value *= _dtab16[q];
	}
	else if (n < 0)
	{
		n = -n;

		value /= _dtab1[n % 16]; 
		q = n / 16;
		if (q >= 20)
		{
			do
			{
				value /= _dtab16[20];
				q -= 20;
			} while (q >= 20);
		}
		value /= _dtab16[q];
	}

	return value;
}

inline static int _estimate_exp10(int exp2)
{
	return (exp2 < 0) ? (int)(exp2 * 0.3010299956639812)
		      : 1 + (int)(exp2 * 0.3010299956639811);		/* log10(2) */
}

static long double d2a_normalize(long double value, int *sign, int *exp10)
{
	union long_double_union v;
	int e10;

	v.d = value;
	*sign = v.ieee.negative;
	*exp10 = 0;
	if (v.ieee.exponent > LONG_DOUBLE_BIAS * 2)
	{
		return value;
	}
	else if (v.ieee.exponent == 0)
	{
		if (v.ieee.mantissa0 == 0 && v.ieee.mantissa1 == 0)
		{
			return value;
		}
		else
		{
			uint64_t f = ((uint64_t)v.ieee.mantissa0 << 32) + v.ieee.mantissa1;
			uint64_t y = (uint64_t)1 << LONG_DOUBLE_MANTISSA_BITS;
			int e = 0 - LONG_DOUBLE_BIAS;
			for (; !(f & y); --e)
				y >>= 1;
			e10 = _estimate_exp10(e);
		}
	}
	else
	{
		e10 = _estimate_exp10(v.ieee.exponent - LONG_DOUBLE_BIAS);
	}

	if (value < 0.0L)
		value = -value;

	if (e10)
		value = _adjust_ld(value, e10);

	if (value < 1.0L)
	{
		value *= 10.0L;
		--e10;
	}

	*exp10 = e10;
	return value;
}

static int d2a_digits(long double value, bool discard0, int *significant, char *buf, size_t size)
{
	int i = 0;
	int carry = 0;
	int prec = *significant;

	if (prec < 0)
	{
		*significant = 0;
		return 0;
	}

	if (prec > (int)size)
		prec = size;

	if (isinf(value))
	{
		buf[i++] = 'i';
	}
	else if (isnan(value))
	{
		buf[i++] = 'n';
	}
	else
	{
		int zero_pos;

		assert(value >= -0.0L && value < 10.0L);

		if (prec < 35)
		{
			value += _adjust_ld(5.0L, prec);
			if (value >= 10.0L)
			{
				value *= 0.1L;
				carry = 1;
				if (prec < (int)size)
					++prec;
			}
		}

		zero_pos = prec;
		for (i = 0; i < prec; ++i)
		{
			int digit = (int)value;
			if (digit)
				zero_pos = prec;
			else if (zero_pos == prec)
				zero_pos = i;
			buf[i] = digit + '0';
			value = value * 10.0L - digit * 10.0L;
		}
		i = discard0 ? zero_pos : prec;
	}
	*significant = i;
	return carry;
}

static int _fraction_hexdigits(uint64_t fraction, int bits, bool uppercase, int *significant, char *buf, size_t size)
{
	uint64_t f;
	int i = 0;
	int carry = 0;
	int prec = *significant;
	char char_a = uppercase ? 'A' : 'a';

	if (prec > (int)size)
		prec = size;

	if (prec < bits / 4)
	{
		f = fraction + ((uint64_t)0x8 << (bits - (prec + 1) * 4));
		if (((f >> (bits - 4)) & 0xf) < ((fraction >> (bits - 4)) & 0xf))
		{
			carry = 1;
			if (prec < (int)size)
				prec++;
			buf[i++] = '1';
		}
	}
	else
	{
		f = fraction;
	}

	for (; i < prec; ++i)
	{
		int shift = (bits - 4 - i * 4);
		int x = shift >= 0 ? (f >> shift) & 0xf : 0;
		if (x < 10)
			buf[i] = x + '0';
		else
			buf[i] = x - 10 + char_a;
	}
	*significant = i;
	return carry;
}

static unsigned int _read_int(const unsigned char **pp)
{
	unsigned int retval = **pp - '0';

	while (isdigit(*++(*pp)))
	{
		retval *= 10;
		retval += **pp - '0';
	}

	return retval;
}

static inline unsigned char *_strchrnul(const unsigned char *str, int ch)
{
#if __GNUC__
	return (unsigned char *)strchrnul((char *)str, ch);
#else
	for (; *str; ++str)
	{
		if (*str == ch)
			break;
	}
	return (unsigned char *)str;
#endif
}

static inline size_t _strnlen(const char *str, size_t max)
{
	if (*str && max)
	{
		const char *p = (char *)memchr(str, 0, max);
		return p ? p - str : max;
	}
	return 0;
}

#define SPECIAL(p, val, Base, digits)				\
	case Base:						\
		do {						\
			*--(p) = (digits)[(val) % Base];	\
		} while (((val) /= Base) != 0);			\
		break

#define ITOA(p, val, base, digits)	do {			\
	switch (base)						\
	{							\
	SPECIAL(p, val, 10, digits);				\
	SPECIAL(p, val, 16, digits);				\
	SPECIAL(p, val, 8, digits);				\
	default: do {						\
			*--(p) = (digits)[(val) % (base)];	\
		} while (((val) /= (base)) != 0);		\
	}							\
} while (0)


static inline char *_itoa_ul(char *bufend, unsigned long value, unsigned int base, bool upper_case)
{
	const char *digits = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";
	ITOA(bufend, value, base, digits);
	return bufend;
}

static inline char *_itoa_ull(char *bufend, unsigned long long value, unsigned int base, bool upper_case)
{
	const char *digits = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";
	ITOA(bufend, value, base, digits);
	return bufend;
}

#undef ITOA
#undef SPECIAL

static char *_exp2a(char *bufend, int exp)
{
	int negative = 0;
	char *p;

	if (exp < 0)
	{
		negative = 1;
		exp = -exp;
	}

	p = _itoa_ul(bufend, exp, 10, 0);

	if (bufend - p == 1)
		*(--p) = '0';
	*(--p) = negative ? '-' : '+';

	return p;
}


ssize_t vxformat(xfmt_callback_function callback,
		xio_write_function xio_write, void *cookie,
		char *o_buf, size_t o_size, const char *fmt, va_list ap)
{
#define OUTCHAR(c)		do {					\
	if (xio_write && o_cur >= (ssize_t)o_size) {			\
		if (xio_write(cookie, o_buf, o_cur) != o_cur) return -1;\
		o_cur = 0;						\
	}								\
	if (o_cur < (ssize_t)o_size)					\
		o_buf[o_cur++] = (c);					\
	++cur_pos;							\
} while(0)


#define OUTSTRING(x, l)		do {					\
    ssize_t _l__ = (l);							\
    if (_l__ > 0) {							\
	char *_s__ = (char *)(x);					\
	if (xio_write && o_cur + _l__ > (ssize_t)o_size) {		\
		if (o_cur && xio_write(cookie, o_buf, o_cur) != o_cur)	\
			return -1;					\
		o_cur = 0;						\
	}								\
	if (xio_write && _l__ >= (ssize_t)o_size) {			\
		if (xio_write(cookie, _s__, _l__) != _l__) return -1;	\
	} else if (o_cur < (ssize_t)o_size) {				\
		char *_d__ = o_buf + o_cur;				\
		ssize_t _n__ = o_size - o_cur;				\
		if (_n__ > _l__) _n__ = _l__;				\
		o_cur += _n__;						\
		if (_n__ <= 32) do *_d__++ = *_s__++; while (--_n__);	\
		else 		memcpy(_d__, _s__, _n__);		\
	}								\
	cur_pos += _l__;						\
    }									\
} while(0)


#define OUTPAD(c, l)		do {					\
    ssize_t _l__ = (l);							\
    if (_l__ > 0) {							\
	char _c__ = (c);						\
	ssize_t _m__ = _l__;						\
	while (xio_write && o_cur + _m__ > (ssize_t)o_size) {		\
		ssize_t _n__ = o_size - o_cur;				\
		memset(o_buf + o_cur, _c__, _n__);			\
		if (xio_write(cookie, o_buf, o_size) != (ssize_t)o_size) \
			return -1;					\
		_m__ -= _n__;						\
		o_cur = 0;						\
	}								\
	if (_m__ && o_cur < (ssize_t)o_size) {				\
		char *_d__ = o_buf + o_cur;				\
		ssize_t _n__ = o_size - o_cur;				\
		if (_n__ > _m__) _n__ = _m__;				\
		o_cur += _n__;						\
		if (_n__ <= 32) do *_d__++ = _c__; while (--_n__);	\
		else		memset(_d__, _c__, _m__);		\
	}								\
	cur_pos += _l__;						\
    }									\
} while(0)


#define JUMPTAB(tab)	do { jmptab = (tab); } while(0)

	union
	{
		ssize_t count;
		struct out_struct out;
		struct buf_struct buf;
	} cb_st;
	iobuf_t ob;
	char work_buf[128];
	ssize_t o_cur = 0;
	ssize_t cur_pos = 0;
	const unsigned char *f = (const unsigned char *)fmt;
	const unsigned char *start_of_spec, *end_of_spec;
	int saved_errno = errno;
	int negative;
	int prec_remain;
	char prefix[4];
	int prefix_len;
	int str_len;
	int n, minlen;
	char *start, *end;

	bool no_extra;
	bool upper;
	int base;
	union
	{
		long l;
		long long ll;
		unsigned long ul;
		unsigned long long ull;
		double d;
		long double ld;
		char *str;
		void *ptr;
	} value;

	if ((ssize_t)o_size < 0)
		o_size = SIZE_MAX / 2;

	cb_st.count = -1;
	if (xio_write == (xio_write_function)-1)
	{
		xio_write = NULL;
		no_extra = true;
	}
	else
	{
		no_extra = false;
	}

again:
	if (no_extra && cur_pos >= o_size)
		return o_size;

	do
	{
		xfmt_spec_t sp;
		JUMP_TABLE_TYPE *jmptab = step0_jumps;

		memset(&sp, 0, sizeof(sp));
		sp.precision = -1;

		end_of_spec = f;
		if (*f && *f != '%')
		{
			f = _strchrnul(f, '%');
			OUTSTRING(end_of_spec, f - end_of_spec);
		}
		if (*f == 0)
			goto all_done;
		start_of_spec = f++;

		while ((sp.specifier = (unsigned char)*f++) != 0)
		{
			switch (WHAT_TODO(sp.specifier, jmptab))
			{
			case REF (flag_space):         	/* for ' ' */
				sp.fl_space = true;
				JUMPTAB(step0_jumps);
				break;

			case REF (flag_plus):          	/* for '+' */
				sp.fl_showsign = true;
				JUMPTAB(step0_jumps);
				break;

			case REF (flag_minus):         	/* for '-' */
				sp.fl_left = true;
				JUMPTAB(step0_jumps);
				break;

			case REF (flag_hash):          	/* for '#' */
				sp.fl_alt = true;
				JUMPTAB(step0_jumps);
				break;

			case REF (flag_zero):          	/* for '0' */
				if (!sp.fl_left)
					sp.fl_padzero = true;
				JUMPTAB(step0_jumps);
				break;

			case REF (flag_quote):         	/* for '\'' */
				sp.fl_group = true;	/* XXX: ignore */
				JUMPTAB(step0_jumps);
				break;

			case REF (flag_i18n):          	/* for 'I' */
				sp.fl_outdigits = true;	/* XXX: ignore */
				JUMPTAB(step0_jumps);
				break;

			case REF (width_asterics):     	/* for '*' */
				sp.width = va_arg(ap, int);
				/* Negative width means left justified.  */
				if (sp.width < 0)
				{
					sp.width = -sp.width;
					sp.fl_padzero = false;
					sp.fl_left = true;
				}
				JUMPTAB(step1_jumps);
				break;

			case REF (width):              	/* for '1'...'9' */
				--f;
				sp.width = _read_int(&f);
				JUMPTAB(step1_jumps);
				break;

			case REF (precision):          	/* for '.' */
				if (*f == '*')
				{
					++f;
					sp.precision = va_arg(ap, int);
					if (sp.precision < 0)
						sp.precision = -1;
				}
				else if (isdigit(*f))
					sp.precision = _read_int(&f);
				else
					sp.precision = 0;
				JUMPTAB(step2_jumps);
				break;

			case REF (mod_half):           	/* for 'h' */
				sp.is_short = true;
				JUMPTAB(step3a_jumps);
				break;

			case REF (mod_halfhalf):       	/* for 'h' */
				sp.is_char = true;
				JUMPTAB(step4_jumps);
				break;

			case REF (mod_long):           	/* for 'l' */
				sp.is_long = true;
				JUMPTAB(step3b_jumps);
				break;

			case REF (mod_longlong):       	/* for 'L', 'q' */
				sp.is_longlong = true;
				JUMPTAB(step4_jumps);
				break;

			case REF (mod_size_t):         	/* for 'z', 'Z' */
				sp.is_longlong = (sizeof(size_t) > sizeof(unsigned long int));
				sp.is_long = (sizeof(size_t) > sizeof(unsigned int));
				JUMPTAB(step4_jumps);
				break;

			case REF (mod_ptrdiff_t):      	/* for 't' */
				sp.is_longlong = (sizeof(ptrdiff_t) > sizeof(unsigned long int));
				sp.is_long = (sizeof(ptrdiff_t) > sizeof(unsigned int));
				JUMPTAB(step4_jumps);
				break;

			case REF (mod_intmax_t):       	/* for 'j' */
				sp.is_longlong = (sizeof(intmax_t) > sizeof(unsigned long int));
				sp.is_long = (sizeof(intmax_t) > sizeof(unsigned int));
				JUMPTAB(step4_jumps);
				break;

			case REF (form_percent):       	/* for '%' */
				OUTCHAR('%');
				goto again;

			case REF (form_integer):       	/* for 'd', 'i' */
				base = 10;

				if (sp.is_longlong)
				{
					value.ll = va_arg(ap, long long);
					negative = value.ll < 0;
					value.ull = negative ? -value.ll : value.ll;
				}
				else 
				{
					if (sp.is_long)
						value.l = va_arg(ap, long);
					else if (sp.is_char)
						value.l = (char)va_arg(ap, int);
					else if (sp.is_short)
						value.l = (short)va_arg(ap, int);
					else
						value.l = (int)va_arg(ap, int);
					negative = value.l < 0;
					value.ul = negative ? -value.l : value.l;
				}

				prefix_len = 0;
				prec_remain = sp.precision;

				if (negative)
					prefix[prefix_len++] = '-';
				else if (sp.fl_showsign)
					prefix[prefix_len++] = '+';
				else if (sp.fl_space)
					prefix[prefix_len++] = ' ';

				goto handle_print_number;

			case REF (form_unsigned):      	/* for 'u' */
				base = 10;
				goto handle_unsigned_integer;

			case REF (form_octal):         	/* for 'o' */
				base = 8;
				goto handle_unsigned_integer;

			case REF (form_hexa):          	/* for 'X', 'x' */
				base = 16;
				/* fall through */

			handle_unsigned_integer:
				if (sp.is_longlong)
					value.ull = va_arg(ap, unsigned long long);
				else if (sp.is_long)
					value.ul = va_arg(ap, unsigned long);
				else if (sp.is_char)
					value.ul = (unsigned char)va_arg(ap, int);
				else if (sp.is_short)
					value.ul = (unsigned short)va_arg(ap, int);
				else
					value.ul = va_arg(ap, unsigned int);

				prefix_len = 0;
				prec_remain = sp.precision;

				if (sp.fl_alt)
				{
					bool not_zero = sp.is_longlong ? value.ull : value.ul;
					if (not_zero)
					{
						if (base == 8)
						{
							prefix[prefix_len++] = '0';
							--prec_remain;
						}
						else if (base == 16)
						{
							prefix[prefix_len++] = '0';
							prefix[prefix_len++] = (sp.specifier == 'X') ? 'X' : 'x';
						}
					}
				}
				/* fall through */

			handle_print_number:
				/* Before this point, 
					value.ull or value.ul, prefix, prefix_len, prec_remain
				   must be set properly.
				 */
				end = work_buf + sizeof(work_buf);
				if (sp.is_longlong)
					start = _itoa_ull(end, value.ull, base, sp.specifier == 'X');
				else
					start = _itoa_ul(end, value.ul, base, sp.specifier == 'X');
				prec_remain -= (end - start);

				minlen = prefix_len + end - start;
				if (prec_remain > 0)
					minlen += prec_remain;

				if (sp.fl_left)
				{
					OUTSTRING(prefix, prefix_len);
					OUTPAD('0', prec_remain);
					OUTSTRING(start, end - start);
					OUTPAD(' ', sp.width - minlen);
				}
				else 
				{
					if (sp.width <= minlen)
					{
						OUTSTRING(prefix, prefix_len);
					}
					else if (sp.precision < 0 && sp.fl_padzero)
					{
						OUTSTRING(prefix, prefix_len);
						OUTPAD('0', sp.width - minlen);
					}
					else
					{
						OUTPAD(' ', sp.width - minlen);
						OUTSTRING(prefix, prefix_len);
					}

					OUTPAD('0', prec_remain);
					OUTSTRING(start, end - start);
				}
				goto again;

			case REF (form_float):         	/* for 'E', 'e', 'F', 'f', 'G', 'g' */
				if (sp.is_longlong)
					value.ld = va_arg(ap, long double);
				else
					value.d = va_arg(ap, double);
				goto handle_float;

			case REF (form_floathex):      	/* for 'A', 'a' */
				if (sp.is_longlong)
					value.ld = va_arg(ap, long double);
				else
					value.d = va_arg(ap, double);
				/* fall through */

			handle_float:
				upper = isupper(sp.specifier);
				if (!(sp.is_longlong ? isfinite(value.ld) : isfinite(value.d)))
				{
					int inf = sp.is_longlong ? isinf(value.ld) : isinf(value.d);
					if (inf)
					{
						work_buf[0] = inf > 0 ? '+' : '-';
						memcpy(work_buf + 1, upper ? "INF" : "inf", 3);
						minlen = 4;
					}
					else
					{
						memcpy(work_buf, upper ? "NAN" : "nan", 3);
						minlen = 3;
					}

					if (sp.fl_left)
					{
						OUTSTRING(work_buf, minlen);
						OUTPAD(' ', sp.width - minlen);
					}
					else
					{
						OUTPAD(' ', sp.width - minlen);
						OUTSTRING(work_buf, minlen);
					}
				}
				else
				{
					char exp_buf[8];
					int exp10, significant;
					int dot, discard0;
					int realprec;
					long double v = sp.is_longlong ? value.ld : (long double)value.d;

					v = d2a_normalize(v, &negative, &exp10);

					prefix_len = 0;
					if (negative)
						prefix[prefix_len++] = '-';
					else if (sp.fl_showsign)
						prefix[prefix_len++] = '+';
					else if (sp.fl_space)
						prefix[prefix_len++] = ' ';

					if (sp.precision == -1)
						sp.precision = 6;

					discard0 = false;
					if (sp.specifier == 'g' || sp.specifier == 'G')
					{
						discard0 = true;
						if (sp.precision == 0)
							sp.precision = 1;

						significant = sp.precision;
						exp10 += d2a_digits(v, discard0, &significant, work_buf, sizeof(work_buf));

						if (exp10 < -4 || exp10 >= sp.precision)
						{
							sp.precision--;
							goto handle_E;
						}
						else
						{
							sp.precision -= 1 + exp10;
							goto handle_F;
						}
					}

					if (sp.specifier == 'f' || sp.specifier == 'F')
					{
						significant = 1 + exp10 + sp.precision;
						exp10 += d2a_digits(v, discard0, &significant, work_buf, sizeof(work_buf));
				handle_F:
						realprec = discard0 ? significant - (1 + exp10) : sp.precision;
						if (realprec < 0)
							realprec = 0;
						dot = (realprec || sp.fl_alt) ? 1 : 0;
						minlen = prefix_len + 1 + dot + (exp10 > 0 ? exp10 : 0) + realprec;

						if (sp.fl_left || sp.width <= minlen)
						{
							OUTSTRING(prefix, prefix_len);
						}
						else if (sp.fl_padzero)
						{
							OUTSTRING(prefix, prefix_len);
							OUTPAD('0', sp.width - minlen);
						}
						else
						{
							OUTPAD(' ', sp.width - minlen);
							OUTSTRING(prefix, prefix_len);
						}

						if (exp10 >= 0)
						{
							n = 1 + exp10;
							if (n > significant)
							{
								OUTSTRING(work_buf, significant);
								OUTPAD('0', n - significant);
								if (dot)
									OUTCHAR('.');
								OUTPAD('0', realprec);
							}
							else
							{
								OUTSTRING(work_buf, n);
								if (dot)
									OUTCHAR('.');
								OUTSTRING(work_buf + n, realprec);
							}
						}
						else
						{
							OUTCHAR('0');
							if (dot)
								OUTCHAR('.');
							n = -(1 + exp10);
							if (n < realprec)
							{
								OUTPAD('0', n);
								n = realprec - n;
								if (n > significant)
								{
									OUTSTRING(work_buf, significant);
									OUTPAD('0', n - significant);
								}
								else
								{
									OUTSTRING(work_buf, n);
								}
							}
							else
							{
								OUTPAD('0', realprec);
							}
						}

						if (!discard0)
							OUTPAD('0', sp.precision - realprec);

						if (sp.fl_left)
							OUTPAD(' ', sp.width - minlen);
					}
					else if (sp.specifier == 'e' || sp.specifier == 'E')
					{
						significant = 1 + sp.precision;
						exp10 += d2a_digits(v, discard0, &significant, work_buf, sizeof(work_buf));
				handle_E:
						end = exp_buf + sizeof(exp_buf);
						start = _exp2a(end, exp10);

						realprec = discard0 ? significant - 1 : sp.precision;
						dot = (realprec || sp.fl_alt) ? 1 : 0;
						minlen = prefix_len + 1 + dot + realprec + 1 + (end - start);
					
						if (sp.fl_left || sp.width <= minlen)
						{
							OUTSTRING(prefix, prefix_len);
						}
						else if (sp.fl_padzero)
						{
							OUTSTRING(prefix, prefix_len);
							OUTPAD('0', sp.width - minlen);
						}
						else
						{
							OUTPAD(' ', sp.width - minlen);
							OUTSTRING(prefix, prefix_len);
						}

						OUTCHAR(work_buf[0]);
						if (dot)
							OUTCHAR('.');
						OUTSTRING(work_buf + 1, realprec);
						if (!discard0)
							OUTPAD('0', sp.precision - realprec);
						OUTCHAR(upper ? 'E' : 'e');
						OUTSTRING(start, end - start);

						if (sp.fl_left)
							OUTPAD(' ', sp.width - minlen);
					}
					else if (sp.specifier == 'a' || sp.specifier == 'A')
					{
						uint64_t fraction;
						int exp2;
						int bits;
						if (sp.is_longlong)
						{
							union long_double_union v;
							v.d = value.ld;
							negative = v.ieee.negative;
							fraction = ((uint64_t)v.ieee.mantissa0 << 32) + v.ieee.mantissa1;
							exp2 = v.ieee.exponent;
							bits = 64;
							if (exp2 == 0)
							{
								exp2 = 1 - LONG_DOUBLE_BIAS;
								for (; bits > 0; bits -= 4)
								{
									if (fraction & ((uint64_t)0xf << (bits - 4)))
										break;
									exp2 -= 4;
								}
							}
							else
							{
								exp2 -= LONG_DOUBLE_BIAS;
							}

							if (sp.precision == -1)
								sp.precision = (bits - 4) / 4;
							exp2 -= 3;
						}
						else
						{
							union ieee754_binary64 v;
							v.d = value.d;
							negative = v.ieee.negative;
							fraction = ((uint64_t)v.ieee.mantissa0 << 32) + v.ieee.mantissa1;
							exp2 = v.ieee.exponent;
							bits = 52;
							if (exp2 == 0)
							{
								exp2 = 1 - IEEE754_BINARY64_BIAS;
								for (; bits > 0; bits -= 4)
								{
									if (fraction & ((uint64_t)0xf << (bits - 4)))
										break;
									exp2 -= 4;
								}
							}
							else
							{
								exp2 -= IEEE754_BINARY64_BIAS;
								fraction |= (uint64_t)1 << 52;
							}

							if (sp.precision == -1)
								sp.precision = bits / 4;
							bits += 4;
						}

						prefix_len = 0;
						if (negative)
							prefix[prefix_len++] = '-';
						else if (sp.fl_showsign)
							prefix[prefix_len++] = '+';
						else if (sp.fl_space)
							prefix[prefix_len++] = ' ';
						prefix[prefix_len++] = '0';
						prefix[prefix_len++] = upper ? 'X' : 'x';

						significant = 1 + sp.precision;
						if (_fraction_hexdigits(fraction, bits, upper, &significant, work_buf, sizeof(work_buf)))
							exp2 += 4;
						end = exp_buf + sizeof(exp_buf);
						start = _exp2a(end, exp2);

						dot = (sp.precision || sp.fl_alt) ? 1 : 0;
						minlen = prefix_len + 1 + dot + sp.precision + 1 + (end - start);

						if (sp.fl_left || sp.width <= minlen)
						{
							OUTSTRING(prefix, prefix_len);
						}
						else if (sp.fl_padzero)
						{
							OUTSTRING(prefix, prefix_len);
							OUTPAD('0', sp.width - minlen);
						}
						else
						{
							OUTPAD(' ', sp.width - minlen);
							OUTSTRING(prefix, prefix_len);
						}

						OUTCHAR(work_buf[0]);
						if (dot)
							OUTCHAR('.');
						OUTSTRING(work_buf + 1, (sp.precision < significant - 1)
									? sp.precision : significant - 1);
						OUTPAD('0', sp.precision - (significant - 1));
						OUTCHAR(upper ? 'P' : 'p');
						OUTSTRING(start, end - start);

						if (sp.fl_left)
							OUTPAD(' ', sp.width - minlen);
					}
				}
				goto again;

			case REF (form_character):     	/* for 'c' */
				if (sp.is_longlong || sp.is_long)
					OUTCHAR((unsigned char)va_arg(ap, int));	/* FIXME  or wchar_t ? */
				else
					OUTCHAR((unsigned char)va_arg(ap, int));
				goto again;

			case REF (form_wcharacter):    	/* for 'C' */
				sp.is_long = true;
				OUTCHAR((unsigned char)va_arg(ap, int));	/* FIXME  or wchar_t ? */
				goto again;

			case REF (form_string):        	/* for 's', 'S' */
				/* XXX: ignore 'S', same as 's' */
				value.str = (char *)va_arg(ap, const char *);
				str_len = -1;
				if (!value.str)
				{
					value.str = "(null)";
					str_len = (sp.precision < 0 || sp.precision >= 6) ? 6 : 0;
				}
				else if (!*value.str)
				{
					str_len = 0;
				}
				/* fall through */

			handle_print_string:
				/* Before this point, 
					value.str, str_len
				   must be set properly.
				 */
				if (str_len < 0)
					str_len = sp.precision < 0 ? strlen(value.str) : _strnlen(value.str, sp.precision);
				else if (sp.precision >= 0 && str_len > sp.precision)
					str_len = sp.precision;

				if (sp.fl_left)
				{
					OUTSTRING(value.str, str_len);
					OUTPAD(' ', sp.width - str_len);
				}
				else
				{
					OUTPAD(' ', sp.width - str_len);
					OUTSTRING(value.str, str_len);
				}

				goto again;

			case REF (form_pointer):       	/* for 'p' */
				value.ptr = va_arg(ap, void *);
				if (callback && f[0] == '{' && f[1] == '>')
				{
					size_t before;

					start = (char *)f + 2;
					end = strstr(start, "<}");
					if (!end || memchr(start, '%', end - start))
						goto handle_pointer;

					if (cb_st.count == -1)
					{
						cb_st.count = 0;
						if (xio_write)
						{
							cb_st.out.write = xio_write;
							cb_st.out.cookie = cookie;
							iobuf_init(&ob, &out_xio, &cb_st.out, (unsigned char *)work_buf, sizeof(work_buf));
						}
						else
						{
							cb_st.buf.buf = o_buf;
							cb_st.buf.used = &o_cur;
							cb_st.buf.size = o_size;
							cb_st.buf.no_extra = no_extra;
							iobuf_init(&ob, &buf_xio, &cb_st.buf, NULL, 0);
						}
					}

					if (xio_write && o_cur)
					{
						if (xio_write(cookie, o_buf, o_cur) != o_cur)
							return -1;
						o_cur = 0;
					}

					sp.num = cur_pos;
					xstr_init(&sp.spec, (unsigned char *)start_of_spec, (const unsigned char *)end + 2 - start_of_spec);
					xstr_init(&sp.ext, (unsigned char *)start, end - start);

					before = cb_st.count;
					n = callback(&ob, &sp, value.ptr);
					if (n < 0 && cb_st.count == before)
						goto handle_pointer;
					else if (cb_st.count == -1)
						return -1;

					if (ob.len > 0)
					{
						if (iobuf_flush(&ob) < 0 || ob.len > 0 || cb_st.count == -1)
							return -1;
					}

					cur_pos += (cb_st.count - before);
					f = (unsigned char *)end + 2;
					goto again;
				}

			handle_pointer:
				sp.precision = -1;
				if (value.ptr)
				{
					base = 16;
					sp.fl_alt = true;
					sp.fl_group = false;
					sp.is_longlong = false;
					sp.is_long = true;
					value.ul = (unsigned long)value.ptr;

					prefix_len = 0;
					prec_remain = sp.precision;

					if (value.ptr)
					{
						prefix[prefix_len++] = '0';
						prefix[prefix_len++] = 'x';
					}
					goto handle_print_number;
				}
				else
				{
					value.str = "(nil)";
					str_len = 5;
					sp.is_long = false;
					goto handle_print_string;
				}

			case REF (form_number):        	/* for 'n' */
				if (sp.is_longlong)
					*(long long int *)va_arg(ap, void *) = cur_pos;
				else if (sp.is_long)
					*(long int *)va_arg(ap, void *) = cur_pos; 
				else if (sp.is_char)
					*(char *)va_arg(ap, void *) = cur_pos;
				else if (sp.is_short)
					*(short int *)va_arg(ap, void *) = cur_pos;
				else
					*(int *)va_arg(ap, void *) = cur_pos;
				goto again;

			case REF (form_strerror):      	/* for 'm' */
				work_buf[0] = 0;
				strerror_r(saved_errno, work_buf, sizeof(work_buf));
				value.str = work_buf;
				str_len = -1;
				sp.is_long = false;
				goto handle_print_string;

			case REF (form_unknown):
				OUTSTRING(start_of_spec, f - start_of_spec);
				goto again;

			default:
				assert(!"Can't reach here!");
			}
		}

		/* The @fmt string ended in the middle of format specification. */
		--f;
		OUTSTRING(start_of_spec, f - start_of_spec);
		end_of_spec = f;
	} while (0);

all_done:
	if (xio_write && o_cur)
	{
		if (xio_write(cookie, o_buf, o_cur) != o_cur)
			return -1;
	}
	return cur_pos;

#undef JUMPTAB
#undef OUTPAD
#undef OUTSTRING
#undef OUTCHAR
}

ssize_t xformat(xfmt_callback_function callback,
		xio_write_function xio_write, void *cookie,
		char *o_buf, size_t o_size, const char *fmt, ...)
{
	va_list ap;
	ssize_t r;

	va_start(ap, fmt);
	r = vxformat(callback, xio_write, cookie, o_buf, o_size, fmt, ap);
	va_end(ap);
	return r;
}

inline ssize_t xfmt_vsnprintf(xfmt_callback_function callback, char *buf, size_t max, const char *fmt, va_list ap)
{
	ssize_t r = vxformat(callback, NULL, NULL, buf, max, fmt, ap);

	if (max)
	{
		if ((ssize_t)max > 0 && r >= (ssize_t)max)
			buf[max - 1] = 0;
		else
			buf[r] = 0;
	}

	return r;
}

ssize_t xfmt_snprintf(xfmt_callback_function callback, char *buf, size_t max, const char *fmt, ...)
{
	va_list ap;
	ssize_t r;

	va_start(ap, fmt);
	r = xfmt_vsnprintf(callback, buf, max, fmt, ap);
	va_end(ap);
	return r;
}

ssize_t xfmt_sprintf(xfmt_callback_function callback, char *buf, const char *fmt, ...)
{
	va_list ap;
	ssize_t r;

	va_start(ap, fmt);
	r = xfmt_vsnprintf(callback, buf, SIZE_MAX, fmt, ap);
	va_end(ap);
	return r;
}

ssize_t xfmt_vsprintf(xfmt_callback_function callback, char *buf, const char *fmt, va_list ap)
{
	return xfmt_vsnprintf(callback, buf, SIZE_MAX, fmt, ap);
}

ssize_t xfmt_iobuf_printf(xfmt_callback_function callback, iobuf_t *ob, const char *fmt, ...)
{
	va_list ap;
	ssize_t r;

	va_start(ap, fmt);
	r = xfmt_iobuf_vprintf(callback, ob, fmt, ap);
	va_end(ap);
	return r;
}

ssize_t xfmt_iobuf_vprintf(xfmt_callback_function callback, iobuf_t *ob, const char *fmt, va_list ap)
{
	char buf[256];
	return vxformat(callback, (xio_write_function)iobuf_write, ob, buf, sizeof(buf), fmt, ap);
}


#ifdef TEST_XFORMAT

static int callback(iobuf_t *ob, const xfmt_spec_t *spec, void *p)
{
	return -1;	/* unknown extension */
}

int main()
{
	char buf[1024];

	xfmt_snprintf(callback, buf, sizeof(buf), 
		"world %f %.2s %c %p{>s<} haha %m!",
		1234.567890,
		"hello", 'B', NULL);
	printf("%s\n", buf);

	xfmt_snprintf(NULL, buf, sizeof(buf), "#%*.*s#\n", -10, 3, "hello");
	printf("%s", buf);

	xfmt_snprintf(NULL, buf, sizeof(buf), "#%#032.2x#\n", 10);
	xfmt_sprintf(NULL, buf, "#%#032.2x#\n", 10);
	printf("%s", buf);
	return 0;
}

#endif
