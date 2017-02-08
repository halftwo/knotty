#ifndef Srp6a_h_
#define Srp6a_h_

#include "XRefCount.h"
#include "mp.h"
#include "xstr.h"
#include "ostk.h"

class Srp6aServer;
class Srp6aClient;
typedef XPtr<Srp6aServer> Srp6aServerPtr;
typedef XPtr<Srp6aClient> Srp6aClientPtr;


class Srp6aBase: virtual public XRefCount
{
	void _init();
protected:
	ostk_t* _ostk;
	uint16_t _ndigits;
	uint16_t _bsize;

	uint16_t _hash_size;
	void (*_hash_func)(uint8_t *digest, const void *input, size_t size);	// default SHA1
	const char *_hash_name;

	mpi_t _N;
	mpi_t _g;
	mpi_t _k;
	xstr_t _N_xs;
	xstr_t _g_xs;
	xstr_t _A;
	xstr_t _B;
	xstr_t _S;
	xstr_t _u;	// SHA1(PAD(A) | PAD(B))
	xstr_t _M1;	// SHA1(PAD(A) | PAD(B) | PAD(S))
	xstr_t _M2;	// SHA1(PAD(A) | M1 | PAD(S))

	mpi_t _alloc_mpi();
	mpi_t _random_mpi();
	xstr_t _alloc_buf();
	xstr_t _alloc_digest();
	bool _is_modzero(const xstr_t& t);	 // t % N == 0
	void _compute_u();

	Srp6aBase();
	Srp6aBase(uintmax_t g, const xstr_t& N, uint16_t bits, const char *hash=NULL);
	Srp6aBase(uintmax_t g, const xstr_t& N, uint16_t bits, const xstr_t& hash);
	Srp6aBase(const Srp6aBase& s);
	virtual ~Srp6aBase();

public:
	bool set_hash(const char *hash);
	bool set_hash(const xstr_t& hash);

	void set_parameters(uintmax_t g, const xstr_t& N, uint16_t bits);
	void set_parameters(const xstr_t& g, const xstr_t& N, uint16_t bits);

	const char *hash_name() const		{ return _hash_name; }

	xstr_t get_g();
	xstr_t get_N();

	virtual xstr_t compute_S() = 0;
	xstr_t compute_M1();
	xstr_t compute_M2();
};


class Srp6aServer: public Srp6aBase
{
	mpi_t _v;
	mpi_t _b;

	void _init_srv();
public:
	Srp6aServer();
	Srp6aServer(const Srp6aServer& s);
	Srp6aServer(uintmax_t g, const xstr_t& N, uint16_t bits, const char *hash=NULL);
	Srp6aServer(uintmax_t g, const xstr_t& N, uint16_t bits, const xstr_t& hash);

	void set_v(const xstr_t& v);

	xstr_t gen_B();

	void set_A(const xstr_t& A);

	virtual xstr_t compute_S();
};


class Srp6aClient: public Srp6aBase
{
	xstr_t _id;
	xstr_t _pass;
	xstr_t _salt;
	mpi_t _x;
	mpi_t _a;
	xstr_t _v_xs;

	void _init_cli();
	void _compute_x();
public:
	Srp6aClient();
	Srp6aClient(const Srp6aClient& s);
	Srp6aClient(uintmax_t g, const xstr_t& N, uint16_t bits, const char *hash=NULL);
	Srp6aClient(uintmax_t g, const xstr_t& N, uint16_t bits, const xstr_t& hash);

	void set_identity(const xstr_t& I, const xstr_t& p);
	void set_identity(const char *I, const char *p);

	void set_salt(const xstr_t& s);
	void gen_salt();

	xstr_t get_salt();

	xstr_t compute_v();

	xstr_t gen_A();

	void set_B(const xstr_t& B);

	virtual xstr_t compute_S();
};


#endif
