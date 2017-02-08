/*
   The implementation is modified from BigDigits:
	http://www.di-mgt.com.au/bigdigits.html
 */
#include "mp.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: mp.c,v 1.18 2015/05/21 08:50:02 gremlin Exp $";
#endif


#ifdef HAVE_UINT128
#	define MAX_DIGIT	UINT64_MAX
#	define HIBITMASK 	UINT64_C(0x8000000000000000)
	typedef __uint128_t DOUBLE_TYPE;
#else
#	define MAX_DIGIT	UINT32_MAX
#	define HIBITMASK 	UINT32_C(0x80000000)
	typedef uint64_t DOUBLE_TYPE;
#endif

#define BITS_PER_DIGIT	MP_BITS_PER_DIGIT

#define ISODD(d) 	((d) & 0x1)


static inline mpi_t _alloc(ostk_t *ostk, size_t ndigits)
{
	return (mpi_t)ostk_alloc(ostk, ndigits * sizeof(MP_DIGIT));
}

static inline mpi_t _dup(ostk_t *ostk, const mpi_t x, size_t ndigits)
{
	return (mpi_t)ostk_copy(ostk, x, ndigits * sizeof(x[0]));
}

static MP_DIGIT _shiftLeft(mpi_t a, const mpi_t b, size_t shift, size_t ndigits);
static MP_DIGIT _shiftRight(mpi_t a, const mpi_t b, size_t shift, size_t ndigits);

/* Returns size of significant digits in a */
inline size_t mp_digit_length(const mpi_t x, size_t ndigits)
{
	size_t n;
	for (n = ndigits; n; --n)
	{
		if (x[n-1])
			break;
	}
	return n;
}

inline size_t mp_bit_length(const mpi_t x, size_t ndigits)
{
	size_t n = mp_digit_length(x, ndigits);

	if (n > 0)
	{
		MP_DIGIT t;
		MP_DIGIT d = x[n-1];
		size_t r = 0;

#if BITS_PER_DIGIT > 64
		if ((t = d>>64) != 0) { r += 64; d = t; }
#endif
#if BITS_PER_DIGIT > 32
		if ((t = d>>32) != 0) { r += 32; d = t; }
#endif
		if ((t = d>>16) != 0) { r += 16; d = t; }
		if ((t = d>> 8) != 0) { r +=  8; d = t; }
		if ((t = d>> 4) != 0) { r +=  4; d = t; }
		if ((t = d>> 2) != 0) { r +=  2; d = t; }
		if ((t = d>> 1) != 0) { r +=  1; d = t; }
		r += d;
		return (n - 1) * BITS_PER_DIGIT + r;
	}
	else
	{
		return 0;
	}
}

size_t mp_from_hex(mpi_t w, size_t ndigits, const char *buf, size_t size)
{
	size_t i;
	size_t n = 0;
	size_t k = 0;

	w[n] = 0;
	for (i = 0; i < size; ++i)
	{
		MP_DIGIT x;
		int ch = (unsigned char)buf[i];
		if (isspace(ch))
			continue;

		if (ch >= '0' && ch <= '9')
			x = (unsigned char)(ch - '0');
		else if ((ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'))
			x = (unsigned char)((ch | 0x20) - 'a') + 10;
		else
			break;

		if (k >= (BITS_PER_DIGIT / 4))
		{
			k = 0;
			++n;
			if (n >= ndigits)
				break;
			w[n] = 0;
		}

		++k;
		w[n] |= (x << (BITS_PER_DIGIT - k * 4));
	}

	if (k > 0)
		++n;

	// reverse
	for (i = n / 2; i-- > 0; )
	{
		MP_DIGIT tmp = w[i];
		w[i] = w[n - 1 - i];
		w[n - 1 - i] = tmp;
	}
	
	for (i = ndigits; i-- > n; )
	{
		w[i] = 0;
	}

	if (k > 0)
	{
		_shiftRight(w, w, BITS_PER_DIGIT - k * 4, ndigits);
	}

	return n;
}

size_t mp_from_buf(mpi_t w, size_t ndigits, const unsigned char buf[], size_t size)
{
	size_t i;
	size_t n = 0;
	const unsigned char *p;

	p = buf + size;
	if (size > sizeof(MP_DIGIT) * ndigits)
		size = sizeof(MP_DIGIT) * ndigits;

	while (size >= sizeof(MP_DIGIT))
	{
		w[n++] =  ((MP_DIGIT)p[-1])
			| ((MP_DIGIT)p[-2] << 8)
			| ((MP_DIGIT)p[-3] << 16)
			| ((MP_DIGIT)p[-4] << 24)
#ifdef HAVE_UINT128
			| ((MP_DIGIT)p[-5] << 32)
			| ((MP_DIGIT)p[-6] << 40)
			| ((MP_DIGIT)p[-7] << 48)
			| ((MP_DIGIT)p[-8] << 56)
#endif
			;

		p -= sizeof(MP_DIGIT);
		size -= sizeof(MP_DIGIT);
	}

	if (size > 0)
	{
		MP_DIGIT k = 0;
		int shift = 0;
		while (size--)
		{
			k |= (MP_DIGIT)(*--p) << shift;
			shift += 8;
		}
		w[n++] = k;
	}

	for (i = ndigits; i-- > n; )
	{
		w[i] = 0;
	}

	return n;
}

static size_t _to_buf(const mpi_t x, size_t ndigits, unsigned char buf[], size_t size, bool pad)
{
	size_t bits = mp_bit_length(x, ndigits);
	size_t need = (bits + 7) / 8;
	size_t i = 0;
	size_t m;
	unsigned char *p;

	if (need < size)
	{
		m = need;
		if (pad)
		{
			memset(buf, 0, size - need);
			p = buf + size;
		}
		else
		{
			p = buf + need;
		}
	}
	else
	{
		m = size;
		p = buf + size;
	}

	while (m >= sizeof(MP_DIGIT))
	{
		MP_DIGIT k = x[i++];
		p[-1] = k;
		p[-2] = k >> 8;
		p[-3] = k >> 16;
		p[-4] = k >> 24;
#ifdef HAVE_UINT128
		p[-5] = k >> 32;
		p[-6] = k >> 40;
		p[-7] = k >> 48;
		p[-8] = k >> 56;
#endif
		p -= sizeof(MP_DIGIT);
		m -= sizeof(MP_DIGIT);
	}

	if (m > 0)
	{
		MP_DIGIT k = x[i++];
		while (m--)
		{
			*--p = k;
			k >>= 8;
		}
	}

	return need;
}

size_t mp_to_buf(const mpi_t x, size_t ndigits, unsigned char buf[], size_t size)
{
	return _to_buf(x, ndigits, buf, size, false);
}

size_t mp_to_padbuf(const mpi_t x, size_t ndigits, unsigned char buf[], size_t size)
{
	return _to_buf(x, ndigits, buf, size, true);
}

static size_t _to_hex(const mpi_t x, size_t ndigits, char buf[], size_t size, bool pad)
{
	static const char *HEX = "0123456789ABCDEF";
	size_t bits = mp_bit_length(x, ndigits);
	size_t need = (bits + 3) / 4;
	size_t i = 0;
	size_t m;
	char *p;

	if (size > 0)
	{
		/* Actual size excluding the terminating '\0'. */
		--size;
		buf[size] = 0;
	}

	if (need < size)
	{
		m = need;
		if (pad)
		{
			memset(buf, '0', size - need);
			p = buf + size;
		}
		else
		{
			p = buf + need;
			*p = 0;		/* terminating zero */
		}
	}
	else
	{
		m = size;
		p = buf + size;
	}

	while (m >= sizeof(MP_DIGIT) * 2)
	{
		unsigned char ch;
		MP_DIGIT k = x[i++];

		ch = k;
		p[-1] = HEX[ch & 0x0f];
		p[-2] = HEX[ch >> 4];

		ch = k >> 8;
		p[-3] = HEX[ch & 0x0f];
		p[-4] = HEX[ch >> 4];

		ch = k >> 16;
		p[-5] = HEX[ch & 0x0f];
		p[-6] = HEX[ch >> 4];

		ch = k >> 24;
		p[-7] = HEX[ch & 0x0f];
		p[-8] = HEX[ch >> 4];

#ifdef HAVE_UINT128
		ch = k >> 32;
		p[-9] = HEX[ch & 0x0f];
		p[-10] = HEX[ch >> 4];

		ch = k >> 40;
		p[-11] = HEX[ch & 0x0f];
		p[-12] = HEX[ch >> 4];

		ch = k >> 48;
		p[-13] = HEX[ch & 0x0f];
		p[-14] = HEX[ch >> 4];

		ch = k >> 56;
		p[-15] = HEX[ch & 0x0f];
		p[-16] = HEX[ch >> 4];
#endif
		p -= sizeof(MP_DIGIT) * 2;
		m -= sizeof(MP_DIGIT) * 2;
	}

	if (m > 0)
	{
		MP_DIGIT k = x[i++];
		unsigned char ch;
		for (; m >= 2; m -= 2)
		{
			ch = k;
			k >>= 8;
			*--p = HEX[ch & 0x0f];
			*--p = HEX[ch >> 4];
		}

		if (m > 0)
		{
			ch = k;
			*--p = HEX[ch & 0x0f];
		}
	}

	return need;
}

size_t mp_to_hex(const mpi_t x, size_t ndigits, char buf[], size_t size)
{
	return _to_hex(x, ndigits, buf, size, false);
}

size_t mp_to_padhex(const mpi_t x, size_t ndigits, char buf[], size_t size)
{
	return _to_hex(x, ndigits, buf, size, true);
}

inline void mp_from_digit(mpi_t w, size_t ndigits, MP_DIGIT x)
{
	while (ndigits-- > 1)
		w[ndigits] = 0;
	w[0] = x;
}

inline void mp_from_uint(mpi_t w, size_t ndigits, uintmax_t x)
{
#if UINTMAX_MAX == MP_DIGIT_MAX
	mp_from_digit(w, ndigits, x);
#else
	int i, k = sizeof(uintmax_t) / sizeof(MP_DIGIT);
	while (ndigits-- > k)
		w[ndigits] = 0;

	for (i = 0; i < k; ++i)
	{
		w[i] = x;
		x >>= (8 * sizeof(MP_DIGIT));
	}
#endif
}

inline int mp_compare(const mpi_t a, const mpi_t b, size_t ndigits)
{
	while (ndigits--)
	{
		if (a[ndigits] > b[ndigits])
			return 1;
		if (a[ndigits] < b[ndigits])
			return -1;
	}

	return 0;
}

inline bool mp_equal(const mpi_t a, const mpi_t b, size_t ndigits)
{
	while (ndigits--)
	{
		if (a[ndigits] != b[ndigits])
			return false;
	}

	return true;
}

inline bool mp_iszero(const mpi_t x, size_t ndigits)
{
	size_t i;

	for (i = 0; i < ndigits; ++i)
	{
		if (x[i] != 0)
			return false;
	}

	return true;
}


int mp_add(mpi_t w, const mpi_t u, const mpi_t v, size_t ndigits)
{
	MP_DIGIT carry = 0;
	size_t j;

	assert(w != v);

	for (j = 0; j < ndigits; j++)
	{
		w[j] = u[j] + carry;
		if (w[j] < carry)
			carry = 1;
		else
			carry = 0;
		
		w[j] += v[j];
		if (w[j] < v[j])
			++carry;
	}

	return carry;
}

/* return 1 if x < y */
int mp_sub(mpi_t w, const mpi_t u, const mpi_t v, size_t ndigits)
{
	MP_DIGIT borrow = 0;
	size_t j;

	assert(w != v);

	for (j = 0; j < ndigits; j++)
	{
		w[j] = u[j] - borrow;
		if (w[j] > MAX_DIGIT - borrow)
			borrow = 1;
		else
			borrow = 0;
		
		w[j] -= v[j];
		if (w[j] > MAX_DIGIT - v[j])
			++borrow;
	}

	return borrow;
}

static inline void _multiply(MP_DIGIT p[2], MP_DIGIT x, MP_DIGIT y)
{
	DOUBLE_TYPE t = (DOUBLE_TYPE)x * (DOUBLE_TYPE)y;
	p[1] = (MP_DIGIT)(t >> BITS_PER_DIGIT);
	p[0] = (MP_DIGIT)t;
}

static inline MP_DIGIT _divide(MP_DIGIT *pq, MP_DIGIT *pr, const MP_DIGIT u[2], MP_DIGIT v)
{
	DOUBLE_TYPE uu = ((DOUBLE_TYPE)u[1] << BITS_PER_DIGIT) | (DOUBLE_TYPE)u[0];
	DOUBLE_TYPE q = uu / (DOUBLE_TYPE)v;
	*pr = (MP_DIGIT)(uu - q * v);
	*pq = (MP_DIGIT)q;
	return (MP_DIGIT)(q >> BITS_PER_DIGIT);
}

void mp_mul(mpi_t w, const mpi_t u, const mpi_t v, size_t ndigits)
{
	size_t i, j;
	assert(w != u && w != v);

	for (i = 0; i < 2 * ndigits; ++i)
		w[i] = 0;

	for (j = 0; j < ndigits; ++j)
	{
		if (v[j] == 0)
		{
			w[j + ndigits] = 0;
		}
		else
		{
			MP_DIGIT k = 0;
			for (i = 0; i < ndigits; i++)
			{
				MP_DIGIT t[2];
				_multiply(t, u[i], v[j]);

				t[0] += k;
				if (t[0] < k)
					t[1]++;
				t[0] += w[i + j];
				if (t[0] < w[i + j])
					t[1]++;

				w[i + j] = t[0];
				k = t[1];
			}	
			w[j + ndigits] = k;
		}
	}
}

inline void mp_square(mpi_t w, const mpi_t x, size_t ndigits)
{
	mp_mul(w, x, x, ndigits);
}


/*	Compute w = w - qv
	where w = (WnW[n-1]...W[0])
	return modified Wn.
*/
static MP_DIGIT _multSub(MP_DIGIT wn, mpi_t w, const mpi_t v, MP_DIGIT q, size_t n)
{
	MP_DIGIT k = 0;
	size_t i;

	if (q == 0)
		return wn;

	for (i = 0; i < n; i++)
	{
		MP_DIGIT t[2];
		_multiply(t, q, v[i]);
		w[i] -= k;
		if (w[i] > MAX_DIGIT - k)
			k = 1;
		else
			k = 0;
		w[i] -= t[0];
		if (w[i] > MAX_DIGIT - t[0])
			k++;
		k += t[1];
	}
	wn -= k;

	return wn;
}

inline void mp_copy(mpi_t w, const mpi_t x, size_t ndigits)
{
	while (ndigits--)
		w[ndigits] = x[ndigits];
}

inline void mp_assign(mpi_t w, size_t ndigits, const mpi_t x, size_t xdigits)
{
	if (xdigits >= ndigits)
	{
		mp_copy(w, x, ndigits);
	}
	else
	{
		size_t i = xdigits;
		while (i--)
			w[i] = x[i];

		i = ndigits;
		while (i-- > xdigits)
			w[i] = 0;
	}
}


static inline bool _qhatTooBig(MP_DIGIT qhat, MP_DIGIT rhat, MP_DIGIT vn2, MP_DIGIT ujn2)
{
	/*	Returns true if Qhat is too big
		i.e. if (Qhat * Vn-2) > (b.Rhat + Uj+n-2)
	*/
	MP_DIGIT t[2];

	_multiply(t, qhat, vn2);
	if (t[1] < rhat)
		return false;
	else if (t[1] > rhat)
		return true;
	else if (t[0] > ujn2)
		return true;

	return false;
}

inline void mp_zero(mpi_t x, size_t ndigits)
{
	while (ndigits--)
		x[ndigits] = 0;
}


static MP_DIGIT _shiftLeft(mpi_t a, const mpi_t b, size_t shift, size_t ndigits)
{
	/* Computes a = b << shift */
	/* [v2.1] Modified to cope with shift > BITS_PERDIGIT */
	size_t i, y, nw, bits;
	MP_DIGIT mask, carry, nextcarry;

	/* Do we shift whole digits? */
	if (shift >= BITS_PER_DIGIT)
	{
		nw = shift / BITS_PER_DIGIT;
		i = ndigits;
		while (i--)
		{
			if (i >= nw)
				a[i] = b[i-nw];
			else
				a[i] = 0;
		}
		/* Call again to shift bits inside digits */
		bits = shift % BITS_PER_DIGIT;
		carry = b[ndigits-nw] << bits;
		if (bits) 
			carry |= _shiftLeft(a, a, bits, ndigits);
		return carry;
	}
	else
	{
		bits = shift;
	}

	/* Construct mask = high bits set */
	mask = ~(MAX_DIGIT >> bits);
	
	y = BITS_PER_DIGIT - bits;
	carry = 0;
	for (i = 0; i < ndigits; i++)
	{
		nextcarry = (b[i] & mask) >> y;
		a[i] = b[i] << bits | carry;
		carry = nextcarry;
	}

	return carry;
}

static MP_DIGIT _shiftRight(mpi_t a, const mpi_t b, size_t shift, size_t ndigits)
{
	/* Computes a = b >> shift */
	/* [v2.1] Modified to cope with shift > BITS_PERDIGIT */
	size_t i, y, nw, bits;
	MP_DIGIT mask, carry, nextcarry;

	/* Do we shift whole digits? */
	if (shift >= BITS_PER_DIGIT)
	{
		nw = shift / BITS_PER_DIGIT;
		for (i = 0; i < ndigits; i++)
		{
			if ((i + nw) < ndigits)
				a[i] = b[i + nw];
			else
				a[i] = 0;
		}
		/* Call again to shift bits inside digits */
		bits = shift % BITS_PER_DIGIT;
		carry = b[nw - 1] >> bits;
		if (bits) 
			carry |= _shiftRight(a, a, bits, ndigits);
		return carry;
	}
	else
	{
		bits = shift;
	}

	/* Construct mask to set low bits */
	/* (thanks to Jesse Chisholm for suggesting this improved technique) */
	mask = ~(MAX_DIGIT << bits);
	
	y = BITS_PER_DIGIT - bits;
	carry = 0;
	i = ndigits;
	while (i--)
	{
		nextcarry = (b[i] & mask) << y;
		a[i] = b[i] >> bits | carry;
		carry = nextcarry;
	}

	return carry;
}


static void _divmod(mpi_t q, mpi_t r, const mpi_t u, size_t udigits, mpi_t v, size_t vdigits)
{	/*	Computes quotient q = u / v and remainder r = u mod v
		where q, r, u are multiple precision digits
		all of udigits and the divisor v is vdigits.

		Ref: Knuth Vol 2 Ch 4.3.1 p 272 Algorithm D.

		Do without extra storage space, i.e. use r[] for
		normalised u[], unnormalise v[] at end, and cope with
		extra digit Uj+n added to u after normalisation.

		WARNING: this trashes q and r first, so cannot do
		u = u / v or v = u mod v.
		It also changes v temporarily so cannot make it const.
	*/
	size_t shift;
	int n, m, j;
	MP_DIGIT bitmask, overflow = 0;
	MP_DIGIT qhat, rhat, t[2];
	mpi_t uu, ww;
	int qhatOK, cmp;

	/* Clear q and r */
	mp_zero(q, udigits);
	mp_zero(r, udigits);

	/* Work out exact sizes of u and v */
	m = (int)mp_digit_length(u, udigits);
	n = (int)mp_digit_length(v, vdigits);
	m -= n;

	assert(n > 0);

	if (n == 1)
	{	/* Use short division instead */
		r[0] = mp_div_digit(q, u, v[0], udigits);
		return;
	}

	if (m < 0)
	{	/* v > u, so just set q = 0 and r = u */
		mp_copy(r, u, udigits);
		return;
	}

	if (m == 0)
	{	/* u and v are the same length */
		cmp = mp_compare(u, v, (size_t)n);
		if (cmp < 0)
		{	/* v > u, as above */
			mp_copy(r, u, udigits);
			return;
		}
		else if (cmp == 0)
		{	/* v == u, so set q = 1 and r = 0 */
			mp_from_digit(q, udigits, 1);
			return;
		}
	}

	/*	In Knuth notation, we have:
		Given
		u = (Um+n-1 ... U1U0)
		v = (Vn-1 ... V1V0)
		Compute
		q = u/v = (QmQm-1 ... Q0)
		r = u mod v = (Rn-1 ... R1R0)
	*/

	/*	Step D1. Normalise */
	/*	Requires high bit of Vn-1
		to be set, so find most signif. bit then shift left,
		i.e. d = 2^shift, u' = u * d, v' = v * d.
	*/
	bitmask = HIBITMASK;
	for (shift = 0; shift < BITS_PER_DIGIT; ++shift)
	{
		if (v[n-1] & bitmask)
			break;
		bitmask >>= 1;
	}

	if (shift)
	{
		/* Normalise v in situ - NB only shift non-zero digits */
		_shiftLeft(v, v, shift, n);
		/* Copy normalised dividend u*d into r */
		overflow = _shiftLeft(r, u, shift, n + m);
	}
	else
	{
		mp_copy(r, u, n + m);
		overflow = 0;
	}

	uu = r;	/* Use ptr to keep notation constant */

	t[0] = overflow;	/* Extra digit Um+n */

	/* Step D2. Initialise j. Set j = m */
	for (j = m; j >= 0; j--)
	{
		/* Step D3. Set Qhat = [(b.Uj+n + Uj+n-1)/Vn-1] 
		   and Rhat = remainder */
		qhatOK = 0;
		t[1] = t[0];	/* This is Uj+n */
		t[0] = uu[j + n - 1];
		overflow = _divide(&qhat, &rhat, t, v[n - 1]);

		/* Test Qhat */
		if (overflow)
		{	/* Qhat == b so set Qhat = b - 1 */
			qhat = MAX_DIGIT;
			rhat = uu[j + n - 1];
			rhat += v[n - 1];
			if (rhat < v[n - 1])	/* Rhat >= b, so no re-test */
				qhatOK = 1;
		}
		/* [VERSION 2: Added extra test "qhat && "] */
		if (qhat && !qhatOK && _qhatTooBig(qhat, rhat, v[n - 2], uu[j + n - 2]))
		{	/* If Qhat.Vn-2 > b.Rhat + Uj+n-2 
			   decrease Qhat by one, increase Rhat by Vn-1
			*/
			qhat--;
			rhat += v[n - 1];
			/* Repeat this test if Rhat < b */
			if (!(rhat < v[n - 1]))
				if (_qhatTooBig(qhat, rhat, v[n - 2], uu[j + n - 2]))
					qhat--;
		}


		/* Step D4. Multiply and subtract */
		ww = &uu[j];
		overflow = _multSub(t[1], ww, v, qhat, (size_t)n);

		/* Step D5. Test remainder. Set Qj = Qhat */
		q[j] = qhat;
		if (overflow)
		{	/* Step D6. Add back if D4 was negative */
			q[j]--;
			overflow = mp_add(ww, ww, v, (size_t)n);
		}

		t[0] = uu[j + n - 1];	/* Uj+n on next round */

	}	/* Step D7. Loop on j */

	/* Clear high digits in uu */
	for (j = n; j < m + n; j++)
		uu[j] = 0;

	/* Step D8. Unnormalise. */

	if (shift)
	{
		_shiftRight(r, r, shift, n);
		_shiftRight(v, v, shift, n);
	}
}


int mp_add_digit(mpi_t w, const mpi_t u, MP_DIGIT v, size_t ndigits)
{
	/*	Calculates w = u + v
		where w, u are multiprecision integers of ndigits each
		and v is a single precision digit.
		Returns carry if overflow.

		Ref: Derived from Knuth Algorithm A.
	*/

	MP_DIGIT k;
	size_t j;

	k = 0;

	/* Add v to first digit of u */
	w[0] = u[0] + v;
	if (w[0] < v)
		k = 1;
	else
		k = 0;

	/* Add carry to subsequent digits */
	for (j = 1; j < ndigits; j++)
	{
		w[j] = u[j] + k;
		if (w[j] < k)
			k = 1;
		else
			k = 0;
	}

	return k;
}

int mp_sub_digit(mpi_t w, const mpi_t u, MP_DIGIT v, size_t ndigits)
{
	/*	Calculates w = u - v
		where w, u are multiprecision integers of ndigits each
		and v is a single precision digit.
		Returns borrow: 0 if u >= v, or 1 if v > u.

		Ref: Derived from Knuth Algorithm S.
	*/

	MP_DIGIT k;
	size_t j;

	k = 0;

	/* Subtract v from first digit of u */
	w[0] = u[0] - v;
	if (w[0] > MAX_DIGIT - v)
		k = 1;
	else
		k = 0;

	/* Subtract borrow from subsequent digits */
	for (j = 1; j < ndigits; j++)
	{
		w[j] = u[j] - k;
		if (w[j] > MAX_DIGIT - k)
			k = 1;
		else
			k = 0;
	}

	return k;
}

MP_DIGIT mp_mul_digit(mpi_t w, const mpi_t u, MP_DIGIT v, size_t ndigits)
{
	/*	Computes product w = u * v
		Returns overflow k
		where w, u are multiprecision integers of ndigits each
		and v, k are single precision digits

		Ref: Knuth Algorithm M.
	*/

	MP_DIGIT k, t[2];
	size_t j;

	if (v == 0) 
	{
		for (j = 0; j < ndigits; j++)
			w[j] = 0;
		return 0;
	}

	k = 0;
	for (j = 0; j < ndigits; j++)
	{
		/* t = x_i * v */
		_multiply(t, u[j], v);
		/* w_i = LOHALF(t) + carry */
		w[j] = t[0] + k;
		/* Overflow? */
		if (w[j] < k)
			t[1]++;
		/* Carry forward HIHALF(t) */
		k = t[1];
	}

	return k;
}

MP_DIGIT mp_div_digit(mpi_t q, const mpi_t u, MP_DIGIT v, size_t ndigits)
{
	/*	Calculates quotient q = u div v
		Returns remainder r = u mod v
		where q, u are multiprecision integers of ndigits each
		and r, v are single precision digits.

		Makes no assumptions about normalisation.
		
		Ref: Knuth Vol 2 Ch 4.3.1 Exercise 16 p625
	*/
	size_t j;
	MP_DIGIT t[2], r;
	size_t shift;
	MP_DIGIT bitmask, overflow, *uu;

	if (ndigits == 0 || v == 0)
		return 0;

	/*	Normalise first */
	/*	Requires high bit of V
		to be set, so find most signif. bit then shift left,
		i.e. d = 2^shift, u' = u * d, v' = v * d.
	*/
	bitmask = HIBITMASK;
	for (shift = 0; shift < BITS_PER_DIGIT; shift++)
	{
		if (v & bitmask)
			break;
		bitmask >>= 1;
	}

	v <<= shift;
	overflow = _shiftLeft(q, u, shift, ndigits);
	uu = q;
	
	/* Step S1 - modified for extra digit. */
	r = overflow;	/* New digit Un */
	j = ndigits;
	while (j--)
	{
		/* Step S2. */
		t[1] = r;
		t[0] = uu[j];
		overflow = _divide(&q[j], &r, t, v);
	}

	/* Unnormalise */
	r >>= shift;
	return r;
}


int mp_compare_digit(const mpi_t a, MP_DIGIT b, size_t ndigits)
{
	size_t i;

	if (ndigits == 0)
		return (b ? -1 : 0);

	for (i = 1; i < ndigits; ++i)
	{
		if (a[i] != 0)
			return 1;
	}

	if (a[0] < b)
		return -1;
	else if (a[0] > b)
		return 1;

	return 0;
}

static inline int _compare(const mpi_t a, size_t m, const mpi_t b, size_t n)
{
	if (m > n)
	{
		do
		{
			--m;
			if (a[m])
				return 1;
		} while (m > n);
	}
	else if (m < n)
	{
		do
		{
			--n;
			if (b[n])
				return -1;
		} while (m < n);
	}

	while (n--)
	{
		if (a[n] > b[n])
			return 1;
		if (a[n] < b[n])
			return -1;
	}

	return 0;
}

void mp_mod(mpi_t r, const mpi_t x, size_t xdigits, const mpi_t m, size_t mdigits, ostk_t *ostk)
{
	/*	Computes r = x mod m
		where r, m are multiprecision integers of length mdigits
		and x is a multiprecision integer of length xdigits.
		r may overlap m.

		Note that r here is only mdigits long, 
		whereas in _divmod it is xdigits long.
	*/
	int cmp = _compare(x, xdigits, m, mdigits);
	if (cmp < 0)
	{
		mp_copy(r, x, mdigits);
	}
	else if (cmp == 0)
	{
		mp_zero(r, mdigits);
	}
	else
	{
		size_t nn = xdigits > mdigits ? xdigits : mdigits;
		mpi_t qq = _alloc(ostk, xdigits);
		mpi_t rr = _alloc(ostk, nn);
		mpi_t mm = _dup(ostk, m, mdigits);

		_divmod(qq, rr, x, xdigits, mm, mdigits);
		mp_copy(r, rr, mdigits);

		ostk_free(ostk, qq);
	}
}

/* w = (x + y) mod m */
void mp_modadd(mpi_t w, const mpi_t x, const mpi_t y, const mpi_t m, size_t ndigits, ostk_t *ostk)
{
	mpi_t p = _alloc(ostk, ndigits + 1);
	p[ndigits] = mp_add(p, x, y, ndigits);
	mp_mod(w, p, ndigits + 1, m, ndigits, ostk);
	ostk_free(ostk, p);
}

/* w = (x - y) mod m */
void mp_modsub(mpi_t w, const mpi_t x, const mpi_t y, const mpi_t m, size_t ndigits, ostk_t *ostk)
{
	mpi_t p = _alloc(ostk, ndigits);
	if (mp_compare(x, y, ndigits) >= 0)
	{
		mp_sub(p, x, y, ndigits);
		mp_mod(w, p, ndigits, m, ndigits, ostk);
	}
	else
	{
		mp_sub(p, y, x, ndigits);
		mp_mod(p, p, ndigits, m, ndigits, ostk);
		if (!mp_iszero(p, ndigits))
			mp_sub(w, m, p, ndigits);
		else
			mp_zero(w, ndigits);
	}
	ostk_free(ostk, p);
}

/* w = (x * y) mod m */
void mp_modmul(mpi_t w, const mpi_t x, const mpi_t y, const mpi_t m, size_t ndigits, ostk_t *ostk)
{
	mpi_t p = _alloc(ostk, ndigits * 2);
	mp_mul(p, x, y, ndigits);
	mp_mod(w, p, ndigits * 2, m, ndigits, ostk);
	ostk_free(ostk, p);
}


static void _modExp_1(mpi_t w, const mpi_t x, const mpi_t e, const mpi_t m, size_t ndigits, ostk_t *ostk);
static void _modExp_windowed(mpi_t yout, const mpi_t g, const mpi_t e, mpi_t mm, size_t ndigits, ostk_t *ostk);

/* w = x^e mod m */
void mp_modexp_lessmem(mpi_t w, const mpi_t x, const mpi_t e, const mpi_t m, size_t ndigits, ostk_t *ostk)
{
	_modExp_1(w, x, e, m, ndigits, ostk);
}

void mp_modexp(mpi_t w, const mpi_t x, const mpi_t e, const mpi_t m, size_t ndigits, ostk_t *ostk)
{
	_modExp_windowed(w, x, e, m, ndigits, ostk);
}

/* MACROS TO DO MODULAR SQUARING AND MULTIPLICATION USING PRE-ALLOCATED TEMPS */
/* Required lengths |y|=|t1|=|t2|=2*n, |m|=n; but final |y|=n */
/* Square: y = (y * y) mod m */
#define MODSQUARETEMP(y, m, n, t1, t2) do { mp_square(t1, y, n); _divmod(t2, y, t1, n * 2, m, n); } while(0)
/* Mult:   y = (y * x) mod m */
#define MODMULTTEMP(y, x, m, n, t1, t2) do { mp_mul(t1, x, y, n); _divmod(t2, y, t1, n * 2, m, n); } while(0)

#define NEXTBITMASK(mask, n) 	do { if (mask == 1) { mask = HIBITMASK; n--; } else { mask >>= 1; } } while(0)

static void _modExp_1(mpi_t yout, const mpi_t x, const mpi_t e, const mpi_t mm, size_t ndigits, ostk_t *ostk)
{
	/*	Computes y = x^e mod m */
	/*	Binary left-to-right method */
	MP_DIGIT mask;
	size_t n = mp_digit_length(e, ndigits);
	mpi_t t1, t2, y, m;

	assert(ndigits != 0);

	/* Catch e==0 => x^0=1 */
	if (0 == n)
	{
		mp_from_digit(yout, ndigits, 1);
		return;
	}

	t1 = _alloc(ostk, 2 * ndigits);
	t2 = _alloc(ostk, 2 * ndigits);
	y = _alloc(ostk, 2 * ndigits);
	m = _dup(ostk, mm, ndigits);

	/* Find second-most significant bit in e */
	for (mask = HIBITMASK; mask > 0; mask >>= 1)
	{
		if (e[n-1] & mask)
			break;
	}
	NEXTBITMASK(mask, n);

	/* Set y = x */
	mp_copy(y, x, ndigits);

	/* For bit j = k-2 downto 0 */
	while (n)
	{
		/* Square y = y * y mod n */
		MODSQUARETEMP(y, m, ndigits, t1, t2);
		if (e[n-1] & mask)
		{	/*	if e(j) == 1 then multiply
				y = y * x mod n */
			MODMULTTEMP(y, x, m, ndigits, t1, t2);
		} 
		
		/* Move to next bit */
		NEXTBITMASK(mask, n);
	}

	/* Return y */
	mp_copy(yout, y, ndigits);

	ostk_free(ostk, t1);
}

/*
SLIDING-WINDOW EXPONENTIATION
Ref: Menezes, chap 14, p616.
k is called the window size.

14.85 Algorithm Sliding-window exponentiation
INPUT: g, e = (e_t.e_{t-1}...e1.e0)_2 with e_t = 1, and an integer k >= 1.
OUTPUT: g^e.
1. Precomputation.
	1.1 g_1 <-- g, g_2 <-- g^2.
	1.2 For i from 1 to (2^{k-1} - 1) do: g_{2i+1} <-- g_{2i-1} * g_2.
2. A <-- 1, i <-- t.
3. While i >= 0 do the following:
	3.1 If e_i = 0 then do: A <-- A^2, i <-- i - 1.
	3.2 Otherwise (e_i != 0), find the longest bitstring e_i.e_{i-1}...e_l such that i-l+1 <= k
and e_l = 1, and do the following:
	A <-- A^{2i-l+1} * g_{(e_i.e_{i-1}...e_l)_2}
	i <-- l - 1.
4. Return(A).
*/

/* 
Optimal values of k for various exponent sizes.
	The references on this differ in their recommendations. 
	These values reflect experiments we've done on our systems.
	You can adjust this to suit your own situation.
*/
static size_t _WindowLenTable[] = 
{
/*  k = 1  2   3   4    5    6     7     8 */
	5, 16, 64, 240, 768, 1024, 2048, 4096
};
#define WINLENTBLMAX (sizeof(_WindowLenTable)/sizeof(_WindowLenTable[0]))

/*	The process used here to read bits into the lookahead buffer could be improved slightly as
	some bits are read in more than once. But we think this function is tricky enough without 
	adding more complexity for marginal benefit.
*/

/* Computes y = g^e mod m using sliding-window exponentiation */
static void _modExp_windowed(mpi_t yout, const mpi_t g, const mpi_t e, mpi_t mm, size_t ndigits, ostk_t *ostk)
{
	size_t nbits;		/* Number of significant bits in e */
	size_t winlen;		/* Window size */ 
	MP_DIGIT mask;		/* Bit mask used for main loop */
	size_t nwd;		/* Digit counter for main loop */
	MP_DIGIT lkamask;	/* Bit mask for lookahead */
	size_t lkanwd;		/* Digit counter for lookahead */
	MP_DIGIT lkabuf;	/* One-digit lookahead buffer */
	size_t lkalen;		/* Actual size of window for current lookahead buffer */
	int in_window;		/* Flag for in-window state */
	mpi_t g2;		/* g2 = g^2 */
	mpi_t t1, t2;		/* Temp big digits, needed for MULT and SQUARE macros */
	mpi_t a;		/* A */
	mpi_t gtable[(1 << (WINLENTBLMAX-1))];	/* Table of ptrs to g1, g3, g5,... */
	size_t ngt;		/* No of elements in gtable */
	size_t idxmult;		/* Index (in gtable) of next multiplier to use: 0=g1, 1=g3, 2=g5,... */
	int aisone;		/* Flag that A == 1 */
	size_t nn;		/* 2 * ndigits */
	size_t i;		/* Temp counter */
	
	/* Get actual size of e */
	nbits = mp_bit_length(e, ndigits);

	/* Catch easy ones */
	if (nbits == 0)
	{	/* g^0 = 1 */
		return mp_from_digit(yout, ndigits, 1);
	}
	else if (nbits == 1)
	{	/* g^1 = g mod m */
		return mp_mod(yout, g, ndigits, mm, ndigits, ostk);
	}

	/* Lookup optimised window length for this size of e */
	for (winlen = 0; winlen < WINLENTBLMAX && winlen < BITS_PER_DIGIT; winlen++)
	{
		if (nbits <= _WindowLenTable[winlen])
			break;
	}

	/* Default to simple L-R method for 1-bit window */
	if (winlen <= 2)
	{
		return _modExp_1(yout, g, e, mm, ndigits, ostk);
	}

	/* Allocate temp vars - NOTE: all are 2n long */
	nn = 2 * ndigits;
	t1 = _alloc(ostk, nn);
	t2 = _alloc(ostk, nn);
	g2 = _alloc(ostk, nn);
	a = _alloc(ostk, nn);

	/* 1. PRECOMPUTATION */
	/* 1.1 g1 <-- g, (we already have g in the input, so just point to it) */
	gtable[0] = (mpi_t)g;
	/* g2 <-- g^2 */
	mp_modmul(g2, gtable[0], gtable[0], mm, ndigits, ostk);

	/* 1.2 For i from 1 to (2^{k-1} - 1) do: g_{2i+1} <-- g_{2i-1} * g_2. */
	/* i.e. we store (g1, g3, g5, g7,...) */
	ngt = ((size_t)1 << (winlen - 1));
	for (i = 1; i < ngt; i++)
	{
		/* NOTE: we need these elements to be 2n digits long for the MODMULTTEMP fn, 
			but the final result is only n digits long */
		gtable[i] = _alloc(ostk, nn);
		mp_copy(gtable[i], gtable[i-1], ndigits);
		MODMULTTEMP(gtable[i], g2, mm, ndigits, t1, t2);
	}

	/* 2. A <-- 1 (use flag) */
	//mpSetDigit(a, 1, ndigits);
	aisone = 1;

	/* Find most significant bit in e */
	nwd = mp_digit_length(e, ndigits);
	for (mask = HIBITMASK; mask > 0; mask >>= 1)
	{
		if (e[nwd-1] & mask)
			break;
	}

	/* i <-- t; 3. While i >= 0 do the following: */
	/* i.e. look at high bit and every subsequent bit L->R in turn */
	lkalen = 0;
	in_window = 0;
	idxmult = 0;
	while (nwd)
	{
		/* We always square each time around */
		/* A <-- A^2 */
		if (!aisone)	/* 1^2 = 1! */
		{
			MODSQUARETEMP(a, mm, ndigits, t1, t2);
		}

		if (!in_window)
		{	/* Do we start another window? */
			if ((e[nwd-1] & mask))
			{	/* Yes, bit is '1', so setup this window */
				in_window = 1;
				/* Read in look-ahead buffer (a single digit) */
				lkamask = mask;
				lkanwd = nwd;
				/* Read in this and the next winlen-1 bits into lkabuf */
				lkabuf = 0x1;	
				for (i = 0; i < winlen-1; i++)
				{
					NEXTBITMASK(lkamask, lkanwd);
					lkabuf <<= 1;
					/* if lkanwd==0 we have passed the end, so just append a zero bit */
					if (lkanwd && (e[lkanwd-1] & lkamask))
					{
						lkabuf |= 0x1;
					}
				}
				/* Compute this window's length */
				/* i.e. keep shifting right until we have a '1' bit at the end */
				for (lkalen = winlen - 1; lkalen > 0; lkalen--, lkabuf >>= 1)
				{
					if (ISODD(lkabuf))
						break;
				}
				/* Set next multipler to use */
				/* idx = (value(buf) - 1) / 2 */
				idxmult = lkabuf >> 1;


			}
			else
			{	/* No, bit is '0', so just loop */
			}
		}
		else
		{	/* We are in a window... */
			/* Has it finished yet? */
			if (lkalen > 0)
			{
				lkalen--;
			}
		}
		/* Are we at end of this window? */
		if (in_window && lkalen < 1)
		{	/* Yes, so compute A <-- A * g_l */
			if (aisone)
			{
				mp_copy(a, gtable[idxmult], ndigits);
				aisone = 0;
			}
			else
			{
				MODMULTTEMP(a, gtable[idxmult], mm, ndigits, t1, t2);
			}
			in_window = 0;
			lkalen = 0;
		}
		NEXTBITMASK(mask, nwd);
	}
	/* Finally, cope with anything left in the final window */
	if (in_window)
	{
		if (aisone)
		{
			mp_copy(a, gtable[idxmult], ndigits);
			aisone = 0;
		}
		else
		{
			MODMULTTEMP(a, gtable[idxmult], mm, ndigits, t1, t2);
		}
	}

	/* 4. Return (A) */
	mp_copy(yout, a, ndigits);
	ostk_free(ostk, t1);
}

#ifdef TEST_MP

#include "hex.h"
#include <stdio.h>

#define BITS		1024
#define ND		MP_NDIGITS(BITS)
#define HEXSIZE		(MP_BUFSIZE(BITS) * 2 + 1)

int main(int argc, char **argv)
{
	/* The following test data are from rfc5054 */
	const char *N_str = "EEAF0AB9 ADB38DD6 9C33F80A FA8FC5E8 60726187 75FF3C0B 9EA2314C \
		9C256576 D674DF74 96EA81D3 383B4813 D692C6E0 E0D5D8E2 50B98BE4 \
		8E495C1D 6089DAD1 5DC7D7B4 6154D6B6 CE8EF4AD 69B15D49 82559B29 \
		7BCF1885 C529F566 660E57EC 68EDBC3C 05726CC0 2FD4CBF4 976EAA9A \
		FD5138FE 8376435B 9FC61D2F C0EB06E3";

	const char *k_str = "7556AA04 5AEF2CDD 07ABAF0F 665C3E81 8913186F";
	const char *x_str = "94B7555A ABE9127C C58CCF49 93DB6CF8 4D16C124";
	const char *u_str = "CE38B959 3487DA98 554ED47D 70A7AE5F 462EF019";

	const char *v_str = "7E273DE8 696FFC4F 4E337D05 B4B375BE B0DDE156 9E8FA00A 9886D812 \
		9BADA1F1 822223CA 1A605B53 0E379BA4 729FDC59 F105B478 7E5186F5 \
		C671085A 1447B52A 48CF1970 B4FB6F84 00BBF4CE BFBB1681 52E08AB5 \
		EA53D15C 1AFF87B2 B9DA6E04 E058AD51 CC72BFC9 033B564E 26480D78 \
		E955A5E2 9E7AB245 DB2BE315 E2099AFB";

	const char *a_str = "60975527 035CF2AD 1989806F 0407210B C81EDC04 E2762A56 AFD529DD DA2D4393";
	const char *b_str = "E487CB59 D31AC550 471E81F0 0F6928E0 1DDA08E9 74A004F4 9E61F5D1 05284D20";

	const char *S_str = "B0DC82BA BCF30674 AE450C02 87745E79 90A3381F 63B387AA F271A10D \
		233861E3 59B48220 F7C4693C 9AE12B0A 6F67809F 0876E2D0 13800D6C \
		41BB59B6 D5979B5C 00A172B4 A2A5903A 0BDCAF8A 709585EB 2AFAFA8F \
		3499B200 210DCC1F 10EB3394 3CD67FC8 8A2F39A4 BE5BEC4E C0A3212D \
		C346D7E4 74B29EDE 8A469FFE CA686E5A";

	char hex[HEXSIZE];

	MP_DIGIT N[ND], g[ND], k[ND];
	MP_DIGIT x[ND], v[ND];
	MP_DIGIT a[ND], b[ND];
	MP_DIGIT A[ND], B[ND], u[ND];
	MP_DIGIT S[ND], S_user[ND], S_host[ND];
	MP_DIGIT T1[ND], T2[ND];	/* temporary */

	ostk_t *ostk = ostk_create(4096*2);

	mp_from_digit(g, ND, 2);
	mp_from_hex(N, ND, N_str, -1);
	mp_from_hex(k, ND, k_str, -1);
	mp_from_hex(x, ND, x_str, -1);
	mp_from_hex(v, ND, v_str, -1);
	mp_from_hex(u, ND, u_str, -1);
	mp_from_hex(a, ND, a_str, -1);
	mp_from_hex(b, ND, b_str, -1);
	mp_from_hex(S, ND, S_str, -1);

	/* compute: A = g^a % N */
	mp_modexp(A, g, a, N, ND, ostk);
	mp_to_padhex(A, ND, hex, sizeof(hex));
	printf("User A = %s\n", hex);

	/* compute: B = k*v + g^b % N */
	mp_modexp(B, g, b, N, ND, ostk);
	mp_modmul(T1, k, v, N, ND, ostk);
	mp_modadd(B, B, T1, N, ND, ostk);
	mp_to_padhex(B, ND, hex, sizeof(hex));
	printf("Host B = %s\n", hex);

	/* compute: S_user = (B - (k * g^x)) ^ (a + (u * x)) % N */
	mp_modexp(T1, g, x, N, ND, ostk);
	mp_modmul(T1, k, T1, N, ND, ostk);
	mp_modsub(T2, B, T1, N, ND, ostk);
	mp_modmul(T1, u, x, N, ND, ostk);
	mp_modadd(T1, a, T1, N, ND, ostk);
	mp_modexp(S_user, T2, T1, N, ND, ostk);
	printf("S_user is %s\n", mp_equal(S, S_user, ND) ? "OK" : "ERR");

	/* compute: S_host = (A * v^u) ^ b % N */
	mp_modexp(T1, v, u, N, ND, ostk);
	mp_modmul(T1, T1, A, N, ND, ostk);
	mp_modexp(S_host, T1, b, N, ND, ostk);
	printf("S_host is %s\n", mp_equal(S, S_host, ND) ? "OK" : "ERR");

	ostk_destroy(ostk);
	return 0;
}

#endif
