#include "Srp6a.h"
#include "Enforce.h"
#include "sha1.h"
#include "sha256.h"
#include "urandom.h"
#include "xnet.h"
#include "hex.h"
#include <alloca.h>
#include <assert.h>

#define RANDOM_SIZE	(512/2/8)

/**
 * The routines comply with RFC 5054 (SRP for TLS), with the following exceptions:
 * The evidence messages 'M1' and 'M2' are computed according to Tom Wu's paper 
 * "SRP-6: Improvements and refinements to the Secure Remote Password protocol",
 * table 5, from 2002. 
**/


Srp6aBase::Srp6aBase()
{
	_init();
}

Srp6aBase::Srp6aBase(const Srp6aBase& s)
{
	_init();

	_ndigits = s._ndigits;
	_bsize = s._bsize;

	_hash_size = s._hash_size;
	_hash_func = s._hash_func;
	_hash_name = s._hash_name;

	if (s._ostk)
	{
		if (s._N)
		{
			_N = _alloc_mpi();
			memcpy(_N, s._N, _ndigits * sizeof(MP_DIGIT));
		}

		if (s._g)
		{
			_g = _alloc_mpi();
			memcpy(_g, s._g, _ndigits * sizeof(MP_DIGIT));
		}

		if (s._k)
		{
			_k = _alloc_mpi();
			memcpy(_k, s._k, _ndigits * sizeof(MP_DIGIT));
		}
	}
}

Srp6aBase::Srp6aBase(uintmax_t g, const xstr_t& N, uint16_t bits, const char *hash)
{
	try {
		_init();
		set_hash(hash);
		set_parameters(g, N, bits);
	}
	catch (...)
	{
		ostk_destroy(_ostk);
		throw;
	}
}

Srp6aBase::Srp6aBase(uintmax_t g, const xstr_t& N, uint16_t bits, const xstr_t& hash)
{
	try {
		_init();
		set_hash(hash);
		set_parameters(g, N, bits);
	}
	catch (...)
	{
		ostk_destroy(_ostk);
		throw;
	}
}

Srp6aBase::~Srp6aBase()
{
	ostk_destroy(_ostk);
}

void Srp6aBase::_init()
{
	_ostk = ostk_create(0);
	_ndigits = 0;
	_bsize = 0;

	_N = NULL;
	_g = NULL;
	_k = NULL;
	_N_xs = xstr_null;
	_g_xs = xstr_null;
	_A = xstr_null;
	_B = xstr_null;
	_S = xstr_null;
	_u = xstr_null;
	_M1 = xstr_null;
	_M2 = xstr_null;
	_K = xstr_null;

	set_hash("SHA256");
}

bool Srp6aBase::set_hash(const char *hash)
{
	if (!hash || *hash == 0)
		return false;

	xstr_t xs = XSTR_C(hash);
	return set_hash(xs);
}

bool Srp6aBase::set_hash(const xstr_t& hash)
{
	if (_N || hash.len == 0)
		return false;

	if (xstr_case_equal_cstr(&hash, "SHA1"))
	{
		_hash_size = 20;
		_hash_func = sha1_checksum;
		_hash_name = "SHA1";
	}
	else if (xstr_case_equal_cstr(&hash, "SHA256"))
	{
		_hash_size = 32;
		_hash_func = sha256_checksum;
		_hash_name = "SHA256";
	}
	else
	{
		return false;
	}
	return true;
}

static void copy_data(uint8_t **ptr, const void *data, size_t len)
{
	memcpy(*ptr, data, len);
	*ptr += len;
}

void Srp6aBase::set_parameters(const xstr_t& g, const xstr_t& N, uint16_t bits)
{
	ENFORCE(bits >= 512);
	ENFORCE(N.len * 8 <= bits);
	ENFORCE(g.len * 8 <= bits);

	_ndigits = MP_NDIGITS(bits);
	_bsize = MP_BUFSIZE(bits);

	_N = _alloc_mpi();
	_g = _alloc_mpi();
	_k = _alloc_mpi();

	mp_from_buf(_g, _ndigits, g.data, g.len);
	mp_from_buf(_N, _ndigits, N.data, N.len);

	int n = mp_digit_length(_N, _ndigits);
	if (n != _ndigits)
		throw XERROR_MSG(XError, "N does not have enough effective digits");

	if (mp_iszero(_g, _ndigits))
		throw XERROR_MSG(XError, "g can't be zero");

	/* Compute: k = SHA1(N | PAD(g)) 
	 */
	uint8_t *buf = (uint8_t*)ostk_alloc(_ostk, _bsize * 2);
	uint8_t *digest = (uint8_t*)ostk_alloc(_ostk, _hash_size);

        mp_to_padbuf(_N, _ndigits, buf, _bsize);
        mp_to_padbuf(_g, _ndigits, buf + _bsize, _bsize);
	_hash_func(digest, buf, _bsize * 2);
        mp_from_buf(_k, _ndigits, digest, _hash_size);
	ostk_free(_ostk, buf);
}

void Srp6aBase::set_parameters(uintmax_t g_, const xstr_t& N, uint16_t bits)
{
	uint8_t buf[sizeof(uintmax_t)];
	xnet_msb_from_uint(buf, sizeof(buf), g_);
	xstr_t g = XSTR_INIT(buf, sizeof(buf));
	set_parameters(g, N, bits);
}

xstr_t Srp6aBase::get_g()
{
	if (_g_xs.len == 0)
	{
		ENFORCE(_g);
		int n = mp_digit_length(_g, _ndigits);
		_g_xs = ostk_xstr_alloc(_ostk, n * sizeof(MP_DIGIT));
		_g_xs.len = mp_to_buf(_g, _ndigits, _g_xs.data, _g_xs.len);
	}
	return _g_xs;
}

xstr_t Srp6aBase::get_N()
{
	if (_N_xs.len == 0)
	{
		ENFORCE(_N);
		_N_xs = _alloc_buf();
		mp_to_padbuf(_N, _ndigits, _N_xs.data, _N_xs.len);
	}
	return _N_xs;
}

xstr_t Srp6aBase::_alloc_buf()
{
	return ostk_xstr_alloc(_ostk, _bsize);
}

xstr_t Srp6aBase::_alloc_digest()
{
	return ostk_xstr_alloc(_ostk, _hash_size);
}

mpi_t Srp6aBase::_alloc_mpi()
{
	return (mpi_t)ostk_alloc(_ostk, _ndigits * sizeof(MP_DIGIT));
}

mpi_t Srp6aBase::_random_mpi()
{
	mpi_t mp = _alloc_mpi();

	uint8_t buf[RANDOM_SIZE];
	urandom_get_bytes(buf, sizeof(buf));
	mp_from_buf(mp, _ndigits, buf, sizeof(buf));

	return mp;
}

bool Srp6aBase::_is_modzero(const xstr_t& x)
{
	bool isZero = false;
	ssize_t i = x.len;
	while (i-- > 0)
	{
		if (x.data[i])
			break;
	}

	if (i < 0)
		return true;

	if (x.len > (ssize_t)(_bsize - 2 * sizeof(MP_DIGIT)))
	{
		ENFORCE(_N);
		mpi_t T = _alloc_mpi();
		mpi_t r = _alloc_mpi();
		mp_from_buf(T, _ndigits, x.data, x.len);
		mp_mod(r, T, _ndigits, _N, _ndigits, _ostk);
		isZero = mp_iszero(r, _ndigits);
		ostk_free(_ostk, T);
	}

	return isZero;
}

void Srp6aBase::_compute_u()
{
	if (_u.len == 0)
	{
		ENFORCE(_A.len && _B.len);
		_u = _alloc_digest();

		 /* Compute: u = SHA1(PAD(A) | PAD(B)) 
		  */
		uint8_t *buf = (uint8_t*)ostk_alloc(_ostk, _bsize * 2);
		uint8_t *p = buf;
		copy_data(&p, _A.data, _A.len);
		copy_data(&p, _B.data, _B.len);
		_hash_func(_u.data, buf, p - buf);
		ostk_free(_ostk, buf);
	}

	ENFORCE(!_is_modzero(_u));
}

xstr_t Srp6aBase::compute_M1()
{
	if (_M1.len == 0)
	{
		ENFORCE(_A.len && _B.len);
		compute_S();
		_M1 = _alloc_digest();

		/* Compute: M1 = SHA1(PAD(A) | PAD(B) | PAD(S))
		 */
		uint8_t *buf = (uint8_t*)ostk_alloc(_ostk, _bsize * 3);
		uint8_t *p = buf;
		copy_data(&p, _A.data, _A.len);
		copy_data(&p, _B.data, _B.len);
		copy_data(&p, _S.data, _S.len);
		_hash_func(_M1.data, buf, p - buf);
		ostk_free(_ostk, buf);
	}
	return _M1;
}

xstr_t Srp6aBase::compute_M2()
{
	if (_M2.len == 0)
	{
		ENFORCE(_A.len);
		compute_S();
		compute_M1();
		_M2 = _alloc_digest();

		/* Compute: M2 = SHA1(PAD(A) | M1 | PAD(S)) 
		 */
		uint8_t *buf = (uint8_t*)ostk_alloc(_ostk, _bsize * 2 + _hash_size);
		uint8_t *p = buf;
		copy_data(&p, _A.data, _A.len);
		copy_data(&p, _M1.data, _M1.len);
		copy_data(&p, _S.data, _S.len);
		_hash_func(_M2.data, buf, p - buf);
		ostk_free(_ostk, buf);
	}
	return _M2;
}

xstr_t Srp6aBase::compute_K()
{
	if (_K.len == 0)
	{
		compute_S();
		_K = _alloc_digest();

		/* Compute: K = SHA1(PAD(S)) 
		 */
		_hash_func(_K.data, _S.data, _S.len);
	}
	return _K;
}

Srp6aServer::Srp6aServer()
{
	_init_srv();
}

Srp6aServer::Srp6aServer(const Srp6aServer& s)
	: Srp6aBase(s)
{
	_init_srv();
}

Srp6aServer::Srp6aServer(uintmax_t g, const xstr_t& N, uint16_t bits, const char *hash)
	: Srp6aBase(g, N, bits, hash)
{
	_init_srv();
}

Srp6aServer::Srp6aServer(uintmax_t g, const xstr_t& N, uint16_t bits, const xstr_t& hash)
	: Srp6aBase(g, N, bits, hash)
{
	_init_srv();
}

void Srp6aServer::_init_srv()
{
	_v = NULL;
	_b = NULL;
}

void Srp6aServer::set_v(const xstr_t& v)
{
	ENFORCE(!_v);
	_v = _alloc_mpi();
	mp_from_buf(_v, _ndigits, v.data, v.len);
}

xstr_t Srp6aServer::gen_B()
{
	if (_B.len == 0)
	{
		ENFORCE(_N);
		/* Compute: B = k*v + g^b % N
		 */
		_b = _random_mpi();
		_B = _alloc_buf();

		mpi_t B = _alloc_mpi();
		mpi_t T = _alloc_mpi();
                mp_modexp(B, _g, _b, _N, _ndigits, _ostk);
                mp_modmul(T, _k, _v, _N, _ndigits, _ostk);
                mp_modadd(B, B, T, _N, _ndigits, _ostk);
		mp_to_padbuf(B, _ndigits, _B.data, _B.len);
		ostk_free(_ostk, B);
	}
	return _B;
}

void Srp6aServer::set_A(const xstr_t& A)
{
	ENFORCE(!_A.len);
	ENFORCE(!_is_modzero(A));
	_A = ostk_xstr_dup(_ostk, &A);
}

Srp6aClient::Srp6aClient()
{
	_init_cli();
}

Srp6aClient::Srp6aClient(const Srp6aClient& s)
	: Srp6aBase(s)
{
	_init_cli();
}

Srp6aClient::Srp6aClient(uintmax_t g, const xstr_t& N, uint16_t bits, const char *hash)
	: Srp6aBase(g, N, bits, hash)
{
	_init_cli();
}

Srp6aClient::Srp6aClient(uintmax_t g, const xstr_t& N, uint16_t bits, const xstr_t& hash)
	: Srp6aBase(g, N, bits, hash)
{
	_init_cli();
}

void Srp6aClient::_init_cli()
{
	_id = xstr_null;
	_pass = xstr_null;
	_salt = xstr_null;
	_a = NULL;
	_x = NULL;
	_v_xs = xstr_null;
}

void Srp6aClient::set_identity(const xstr_t& I, const xstr_t& p)
{
	ENFORCE(!_id.len && !_pass.len);
	_id = ostk_xstr_dup(_ostk, &I);
	_pass = ostk_xstr_dup(_ostk, &p);
}

void Srp6aClient::set_identity(const char* I, const char* p)
{
	ENFORCE(!_id.len && !_pass.len);
	_id = ostk_xstr_dup_cstr(_ostk, I);
	_pass = ostk_xstr_dup_cstr(_ostk, p);
}

void Srp6aClient::set_salt(const xstr_t& s)
{
	ENFORCE(!_salt.len);
	_salt = ostk_xstr_dup(_ostk, &s);
}

void Srp6aClient::gen_salt()
{
	ENFORCE(!_salt.len);
	_salt = ostk_xstr_alloc(_ostk, 24);
	urandom_get_bytes(_salt.data, _salt.len);
}

xstr_t Srp6aClient::get_salt()
{
	ENFORCE(_salt.len);
	return _salt;
}

void Srp6aClient::_compute_x()
{
	if (!_x)
	{
		ENFORCE(_id.len && _pass.len && _salt.len);
		_x = _alloc_mpi();

		/* Compute: x = SHA1(s | SHA1(I | ":" | p)) 
		 */
		int len1 = _id.len + 1 + _pass.len;
		int len2 = _salt.len + _hash_size;
		uint8_t *buf = (uint8_t*)ostk_alloc(_ostk, XS_MAX(len1, len2));
		uint8_t *digest = (uint8_t*)ostk_alloc(_ostk, _hash_size);
		uint8_t *p;

		p = buf;
		copy_data(&p, _id.data, _id.len);
		*p++ = ':';
		copy_data(&p, _pass.data, _pass.len);
		_hash_func(digest, buf, p - buf);

		p = buf;
		copy_data(&p, _salt.data, _salt.len);
		copy_data(&p, digest, _hash_size);
		_hash_func(digest, buf, p - buf);

                mp_from_buf(_x, _ndigits, digest, _hash_size);
		ostk_free(_ostk, buf);
	}
}

xstr_t Srp6aClient::compute_v()
{
	if (_v_xs.len == 0)
	{
		ENFORCE(_N);
		_compute_x();
		_v_xs = _alloc_buf();

		/* Compute: v = g^x % N 
		 */
		mpi_t v = _alloc_mpi();
		mp_modexp(v, _g, _x, _N, _ndigits, _ostk);
		mp_to_padbuf(v, _ndigits, _v_xs.data, _v_xs.len);
		ostk_free(_ostk, v);
	}
	return _v_xs;
}

xstr_t Srp6aClient::gen_A()
{
	if (_A.len == 0)
	{
		ENFORCE(_N);
		/* Compute: A = g^a % N 
		 */
		_a = _random_mpi();
		_A = _alloc_buf();

		mpi_t A = _alloc_mpi();
		mp_modexp(A, _g, _a, _N, _ndigits, _ostk);
		mp_to_padbuf(A, _ndigits, _A.data, _A.len);
		ostk_free(_ostk, A);
	}
	return _A;
}

void Srp6aClient::set_B(const xstr_t& B)
{
	ENFORCE(!_B.len);
	ENFORCE(!_is_modzero(B));
	_B = ostk_xstr_dup(_ostk, &B);
}

xstr_t Srp6aServer::compute_S()
{
	if (_S.len == 0)
	{
		ENFORCE(_A.len && _v);
		gen_B();
		_compute_u();
		_S = _alloc_buf();

 		/* Compute: S_host = (A * v^u) ^ b % N
		 */
		mpi_t u = _alloc_mpi();
		mpi_t A = _alloc_mpi();
		mpi_t T1 = _alloc_mpi();
		mpi_t T2 = _alloc_mpi();
		mp_from_buf(u, _ndigits, _u.data, _u.len);
		mp_from_buf(A, _ndigits, _A.data, _A.len);
                mp_modexp(T1, _v, u, _N, _ndigits, _ostk);
                mp_modmul(T2, T1, A, _N, _ndigits, _ostk);
                mp_modexp(T1, T2, _b, _N, _ndigits, _ostk);
		mp_to_padbuf(T1, _ndigits, _S.data, _S.len);
		ostk_free(_ostk, u);
	}
	return _S;
}

xstr_t Srp6aClient::compute_S()
{
	if (_S.len == 0)
	{
		ENFORCE(_B.len);
		_compute_x();
		gen_A();
		_compute_u();
		_S = _alloc_buf();

		/* Compute: S_user = (B - (k * g^x)) ^ (a + (u * x)) % N 
 		 */
		mpi_t u = _alloc_mpi();
		mpi_t B = _alloc_mpi();
		mpi_t T1 = _alloc_mpi();
		mpi_t T2 = _alloc_mpi();
		mpi_t TT = (mpi_t)ostk_alloc(_ostk, 2 * _ndigits * sizeof(MP_DIGIT));
		mp_from_buf(u, _ndigits, _u.data, _u.len);
		mp_from_buf(B, _ndigits, _B.data, _B.len);
		mp_modexp(T1, _g, _x, _N, _ndigits, _ostk);
                mp_modmul(T1, _k, T1, _N, _ndigits, _ostk);
                mp_modsub(T2, B, T1, _N, _ndigits, _ostk);

		/* NB: Because u and x have less than ND/2 digits, (u * x)
                       has no more than ND digits.
                       TT has 2*ND digits, but the higher ND digits are all zeros.
                 */
		mp_mul(TT, u, _x, _ndigits);

		if (mp_add(T1, TT, _a, _ndigits))	/* overflow */
		{
			/* T ^ (P + Q) == (T ^ P) * (T ^ Q)
			 */
			mpi_t T3 = _alloc_mpi();
			mpi_t T4 = _alloc_mpi();

			mp_add_digit(T1, T1, 1, _ndigits);
			mp_modexp(T3, T2, T1, _N, _ndigits, _ostk);

			mp_zero(T1, _ndigits);
			mp_sub_digit(T1, T1, 1, _ndigits);
			mp_modexp(T4, T2, T1, _N, _ndigits, _ostk);

			mp_modmul(T1, T3, T4, _N, _ndigits, _ostk);
		}
		else
		{
			mp_modexp(T1, T2, T1, _N, _ndigits, _ostk);
		}

		mp_to_padbuf(T1, _ndigits, _S.data, _S.len);

		ostk_free(_ostk, u);
	}
	return _S;
}


#ifdef TEST_SRP6A

#include <stdio.h>
#define BITS	2048

int main(int argc, char **argv)
{
	const char *N_str = \
	"ac6bdb41324a9a9bf166de5e1389582faf72b6651987ee07fc3192943db56050"
	"a37329cbb4a099ed8193e0757767a13dd52312ab4b03310dcd7f48a9da04fd50"
	"e8083969edb767b0cf6095179a163ab3661a05fbd5faaae82918a9962f0b93b8"
	"55f97993ec975eeaa80d740adbf4ff747359d041d5c33ea71d281e446b14773b"
	"ca97b43a23fb801676bd207a436c6481f1d2b9078717461a5b9d32e688f87748"
	"544523b524b0d57d5ea77a2775d2ecfa032cfbdbf52fb3786160279004e57ae6"
	"af874e7303ce53299ccc041c7bc308d82a5698f3a8d0c38271ae35f8e9dbfbb6"
	"94b5c803d89f7ae435de236d525f54759b65e372fcd68ef20fa7111f9e4aff73";

	uint8_t N_buf[MP_BUFSIZE(BITS)];
	int N_len = unhexlify(N_buf, N_str, -1);
	assert(N_len > 0);
	xstr_t N = XSTR_INIT(N_buf, N_len);

	uint8_t ibuf[16], pbuf[16];
	xstr_t identity = XSTR_INIT(ibuf, sizeof(ibuf));
	xstr_t password = XSTR_INIT(pbuf, sizeof(pbuf));

	for (int i = 0; i < 1000; ++i)
	{
		Srp6aServerPtr srv = new Srp6aServer(2, N, BITS, "SHA256");
		Srp6aClientPtr cli = new Srp6aClient(2, N, BITS, "SHA256");

		urandom_get_bytes(identity.data, identity.len);
		urandom_get_bytes(password.data, password.len);

		cli->set_identity(identity, password);
		cli->gen_salt();

		xstr_t v = cli->compute_v();
		srv->set_v(v);

		xstr_t A = cli->gen_A();
		xstr_t B = srv->gen_B();

		cli->set_B(B);
		srv->set_A(A);

		xstr_t S1 = cli->compute_S();
		xstr_t S2 = srv->compute_S();
	
		if (!xstr_equal(&S1, &S2))
		{
			fprintf(stderr, "S differ\n");
			exit(1);
		}
	}

	return 0;
}

#endif
