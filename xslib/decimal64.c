#include "decimal64.h"
#include <string.h>
#include <ctype.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id$";
#endif

const decimal64_t decimal64_zero = {
#if	__BYTE_ORDER == __BIG_ENDIAN
	.bytes = { 0x22, 0x38, 0, 0, 0, 0, 0, 0 }
#elif	__BYTE_ORDER == __LITTLE_ENDIAN
	.bytes = { 0, 0, 0, 0, 0, 0, 0x38, 0x22 }
#endif
};

const decimal64_t decimal64_minus_zero = {
#if	__BYTE_ORDER == __BIG_ENDIAN
	.bytes = { 0xa2, 0x38, 0, 0, 0, 0, 0, 0 }
#elif	__BYTE_ORDER == __LITTLE_ENDIAN
	.bytes = { 0, 0, 0, 0, 0, 0, 0x38, 0xa2 }
#endif
};


size_t decimal64_to_cstr(decimal64_t value, char buf[])
{
	uint8_t bcd[DECDOUBLE_Pmax];
	int32_t expo, sign;

	sign = decDoubleToBCD(&value, &expo, bcd);
	if (expo > DECDOUBLE_Emax || expo < -DECDOUBLE_Bias)
	{
		decDoubleToString(&value, buf);
		return strlen(buf);
	}
	else
	{
		int start, end;
		char *p = buf;
		for (start = 0; start < DECDOUBLE_Pmax && (bcd[start] == 0); ++start)
		{
			continue;
		}

		for (end = DECDOUBLE_Pmax; end > start && (bcd[end - 1] == 0); --end)
		{
			continue;
		}

		if (sign)
			*p++ = '-';

		if (start >= DECDOUBLE_Pmax)
		{
			*p++ = '0';
		}
		else if (expo >= 0)
		{
			int i;
			if (expo > start)
			{
				i = start;
				*p++ = '0' + bcd[i++];
				if (i < end)
				{
					*p++ = '.';
					do {
						*p++ = '0' + bcd[i++];
					} while (i < end);
				}

				*p++ = 'E';
				*p++ = '+';
				p += sprintf(p, "%d", expo + (DECDOUBLE_Pmax - 1 - start));
			}
			else if (expo == 0 && start >= DECDOUBLE_Pmax)
			{
				*p++ = '0';
			}
			else
			{
				i = start;
				while (i < DECDOUBLE_Pmax)
					*p++ = '0' + bcd[i++];

				for (i = 0; i < expo; ++i)
					*p++ = '0';
			}
		}
		else
		{
			int i, k;
			expo += DECDOUBLE_Pmax - 1 - start;

			i = start;
			*p++ = '0' + bcd[i++];
			for (k = 0; k < expo; ++k)
				*p++ = '0' + bcd[i++];

			if (i < end)
			{
				*p++ = '.';
				do {
					*p++ = '0' + bcd[i++];
				} while (i < end);
			}

			if (expo < 0)
			{
				*p++ = 'E';
				p += sprintf(p, "%d", expo);
			}
		}

		*p = 0;
		return p - buf;
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

/* NB:
 * Round policy is ROUND_HALF_EVEN 
 */
int decimal64_to_integer(decimal64_t dec, intmax_t *pv)
{
	uint8_t bcd[DECDOUBLE_Pmax];
	int32_t expo, sign;
	intmax_t v;
	int error = 0;

	sign = decDoubleToBCD(&dec, &expo, bcd);
	if (expo > DECDOUBLE_Emax || expo < -DECDOUBLE_Bias)
	{
		v = INTMAX_MAX;
		if (expo == DECFLOAT_Inf)
			error = DECIMAL64_ERR_OVERFLOW;
		else if (expo == DECFLOAT_qNaN)
			error = DECIMAL64_ERR_NAN;
		else if (expo == DECFLOAT_sNaN)
			error = DECIMAL64_ERR_NAN;
	}
	else
	{
		intmax_t significand = 0;
		int i, digits;
		for (i = 0; i < DECDOUBLE_Pmax && (bcd[i] == 0); ++i)
		{
			continue;
		}

		digits = DECDOUBLE_Pmax - i;

		for (; i < DECDOUBLE_Pmax; ++i)
		{
			significand *= 10;
			significand += bcd[i];
		}

		v = significand;
		if (significand)
		{
			if (expo + digits < 0)
			{
				v = 0;
				error = DECIMAL64_ERR_UNDERFLOW;
			}
			else if (expo + digits == 0)
			{
				uint8_t *first = &bcd[DECDOUBLE_Pmax - digits];
				if (*first > 5 || (*first == 5 && !is_zero(first+1, digits-1)))
				{
					v = 1;
					error = DECIMAL64_ERR_ROUND;
				}
				else
				{
					v = 0;
					error = DECIMAL64_ERR_UNDERFLOW;
				}
			}
			else if (expo < 0)
			{
				int d1 = 0, d2 = 0;
				for (; expo < 0; ++expo)
				{
					if (d1)
						d2 = d1;

					d1 = v % 10;
					v /= 10;
				}

				if (d1 || d2)
				{
					if (d1 > 5 || (d1 == 5 && (d2 || v % 2)))
					{
						v += 1;
					}
					error = DECIMAL64_ERR_ROUND;
				}
			}
			else
			{
				for (; expo > 0; --expo)
				{
					if (v > INTMAX_MAX/10)
					{
						v = INTMAX_MAX;
						error = DECIMAL64_ERR_OVERFLOW;
						break;
					}
					v *= 10;
				}
			}
		}
	}

	*pv = sign ? -v : v;
	return error;
}

/* Return true if overflow. */
static bool _round_half_even(uint8_t *bcd, int size, int point)
{
	if (bcd[point] > 5 || (bcd[point] == 5 && (!is_zero(bcd + point + 1, size - point - 1) || bcd[point-1]%2)))
	{
		/* round up */
		int i;
		for (i = point - 1; i >= 0; --i)
		{
			if (bcd[i] < 9)
			{
				bcd[i] += 1;
				break;
			}

			bcd[i] = 0;
		}
		return (i < 0);
	}
	return false;
}

int decimal64_from_xstr(decimal64_t *dec, const xstr_t *xs, xstr_t *end/*NULL*/)
{
	uint8_t bcd[DECDOUBLE_Pmax+32];
	int32_t expo = 0, sign = 0;
	bool expo_negative = false;
	bool has_fraction = false;
	unsigned char *s, *invalid, *last;
	int n, k = 0, point = 0;
	int error = 0;
	enum
	{
		BEFORE_POINT,
		AFTER_POINT,
		EXPONENT_MINUS,
		EXPONENT_DIGIT,
	} state = BEFORE_POINT;

	s = (unsigned char *)xs->data;
	invalid = s;
	last = s + xs->len;
	memset(bcd, 0, sizeof(bcd));

	while (s < last && isspace(*s))
		++s;

	if (s >= last)
		goto done;

	if (*s == '-')
	{
		sign = DECFLOAT_Sign;
		if (++s >= last)
			goto done;
	}
	else if (*s == '+')
	{
		if (++s >= last)
			goto done;
	}

	if (*s == 'i' || *s == 'I')
	{
		xstr_t str = XSTR_INIT((unsigned char *)s, last - s);
		if (xstr_case_start_with_cstr(&str, "inf"))
		{
			expo = DECFLOAT_Inf;
			if (xstr_case_start_with_cstr(&str, "infinity"))
				invalid = s + 8;
			else
				invalid = s + 3;
		}
		goto done;
	}
	else if (*s == 'q' || *s == 'Q' || *s == 's' || *s == 'S' || *s == 'n' || *s == 'N')
	{
		bool signalling = false;
		xstr_t str;
		if (*s != 'n' && *s != 'N')
		{
			if (*s == 's' || *s == 'S')
				signalling = true;
			++s;
		}

		xstr_init(&str, (unsigned char *)s, last - s);
		if (xstr_case_start_with_cstr(&str, "nan"))
		{
			expo = signalling ? DECFLOAT_sNaN : DECFLOAT_qNaN;
			s += 3;
			invalid = s;
			if (s < last && *s == '(')
			{
				while (++s < last && isalnum(*s))
					continue;

				if (s < last && *s == ')')
					invalid = s + 1;
			}
		}
		goto done;
	}

	for (; s < last; ++s)
	{
		switch (*s)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':

			n = (*s - '0');

			if (state == BEFORE_POINT)
			{
				has_fraction = true;
				if (k > 0 || n > 0)
				{
					++point;
					if (k < sizeof(bcd))
						bcd[k++] = n;
					else
						error = DECIMAL64_ERR_LOST_DIGITS;
				}
			}
			else if (state == AFTER_POINT)
			{
				has_fraction = true;
				if (k < sizeof(bcd))
					bcd[k++] = n;
				else
					error = DECIMAL64_ERR_LOST_DIGITS;
			}
			else
			{
				if (state == EXPONENT_MINUS)
					state = EXPONENT_DIGIT;

				if (expo < (INT32_MAX - 9) / 10)	// overflow
					expo = expo * 10 + n;
			}
			invalid = s + 1;
			break;

		case '.':
			if (state != BEFORE_POINT)
				goto done;
			state = AFTER_POINT;
			if (has_fraction)
				invalid = s + 1;
			break;

		case 'e':
		case 'E':
			if (state >= EXPONENT_MINUS || !has_fraction)
				goto done;
			state = EXPONENT_MINUS;
			break;

		case '-':
			expo_negative = true;
		case '+':
			if (state != EXPONENT_MINUS)
				goto done;
			state = EXPONENT_DIGIT;
			break;

		default:
			goto done;
		}
	}

done:
	if (end)
		xstr_init(end, (unsigned char *)invalid, last - invalid);

	if (expo_negative)
		expo = -expo;

	expo += point - DECDOUBLE_Pmax;
	if (expo > DECDOUBLE_Emax - (DECDOUBLE_Pmax - 1))
	{
		memset(bcd, 0, sizeof(bcd));
		expo = DECFLOAT_Inf;
		error = DECIMAL64_ERR_OVERFLOW;
	}
	else if (expo < -DECDOUBLE_Bias)
	{
		int shift = -DECDOUBLE_Bias - expo;
		if (shift > DECDOUBLE_Pmax)
		{
			memset(bcd, 0, sizeof(bcd));
			expo = 0;
			error = DECIMAL64_ERR_UNDERFLOW;
		}
		else
		{
			memmove(bcd + shift, bcd, sizeof(bcd) - shift);
			memset(bcd, 0, shift);
			_round_half_even(bcd, sizeof(bcd), DECDOUBLE_Pmax);
			expo += shift;
			error = DECIMAL64_ERR_ROUND;
/*
			if (shift == DECDOUBLE_Pmax && bcd[DECDOUBLE_Pmax-1]==0)
			{
				expo = 0;
				error = DECIMAL64_ERR_UNDERFLOW;
			}
*/
		}
	}
	else if (k > DECDOUBLE_Pmax)
	{
		if (_round_half_even(bcd, sizeof(bcd), DECDOUBLE_Pmax))
		{
			bcd[0] = 1;
			expo += 1;
			error = DECIMAL64_ERR_ROUND;
			if (expo > DECDOUBLE_Emax - (DECDOUBLE_Pmax - 1))
			{
				memset(bcd, 0, sizeof(bcd));
				expo = DECFLOAT_Inf;
				error = DECIMAL64_ERR_OVERFLOW;
			}
		}
		else
		{
			error = DECIMAL64_ERR_ROUND;
		}
	}

	decDoubleFromBCD(dec, expo, bcd, sign);
	return error;
}

int decimal64_from_cstr(decimal64_t *dec, const char *str, char **end/*NULL*/)
{
	xstr_t xsend;
	xstr_t xs = XSTR_C(str);
	int rc = decimal64_from_xstr(dec, &xs, &xsend);
	if (end)
	{
		*end = (char *)xsend.data;
	}
	return rc;
}

int decimal64_from_integer(decimal64_t *dec, intmax_t v)
{
	bool negative = false;
	int expo = 0;
	uint8_t bcd[DECDOUBLE_Pmax];
	bool lost_digits = false;
	int i;

	if (v < 0)
	{
		negative = true;
		v = -v;
	}

	for (i = DECDOUBLE_Pmax; i > 0 && v; --i)
	{
		bcd[i-1] = v % 10;
		v /= 10;
	}

	while (v)
	{
		if (bcd[sizeof(bcd) - 1])
			lost_digits = true;

		memmove(bcd + 1, bcd, sizeof(bcd) - 1);
		bcd[0] = v % 10;
		v /= 10;
		++expo;
	}

	decDoubleFromBCD(dec, expo, bcd, negative ? DECFLOAT_Sign : 0);
	return lost_digits ? DECIMAL64_ERR_LOST_DIGITS : 0;
}


#ifdef TEST_DECIMAL64

#include <stdio.h>

int main(int argc, char **argv)
{
	const char *strs[] = {
		"0.500567890123456789E-398",
		"1.234567890123456789E+378",
		"0009.9999999999999997654321E+383",
		"1.234567890123456789E-383",
		"-Inf",
	};
	int i;

	for (i = 0; i < XS_ARRCOUNT(strs); ++i)
	{
		char buf1[DECIMAL64_STRING_MAX];
		char buf2[DECIMAL64_STRING_MAX];
		char buf3[DECIMAL64_STRING_MAX];
		const char *str = strs[i];
		char *end;
		decContext ctx;
		decimal64_t d1, d2;

		decContextDefault(&ctx, DEC_INIT_DECIMAL64);
		decDoubleFromString(&d1, str, &ctx);
		decimal64_to_cstr(d1, buf1);

		decimal64_from_cstr(&d2, str, &end);
		decimal64_to_cstr(d2, buf2);

		decDoubleToString(&d2, buf3);

		if (strcmp(buf1, buf2) != 0 || strcmp(buf1, buf3) != 0)
		{
			printf("str =%s\n", str);
			printf("buf1=%s\n", buf1);
			printf("buf2=%s\n", buf2);
			printf("buf3=%s\n", buf3);
		}
	}
	return 0;
}

#endif
