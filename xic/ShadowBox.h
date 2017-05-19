#ifndef ShadowBox_h_
#define ShadowBox_h_

#include "xslib/XRefCount.h"
#include "xslib/xstr.h"
#include "xslib/ostk.h"
#include "xslib/xio.h"
#include "xslib/Srp6a.h"
#include <string>
#include <map>

class ShadowBox;
typedef XPtr<ShadowBox> ShadowBoxPtr;

class ShadowBox: virtual public XRefCount
{
	struct Srp6aInfo
	{
		xstr_t hashId;
		int bits;
		uintmax_t g;
		xstr_t N;
		Srp6aServerPtr srp6a;

		Srp6aInfo(const xstr_t& hid, int b_, uintmax_t g_, const xstr_t& N_)
			: hashId(hid), bits(b_), g(g_), N(N_)
		{
			srp6a = new Srp6aServer(g, N, bits, hashId);
		}
	};

	struct Verifier
	{
		xstr_t method;
		xstr_t paramId;
		xstr_t s;
		xstr_t v;

		Verifier(const xstr_t& m_, const xstr_t& pid, const xstr_t& s_, const xstr_t& v_)
			: method(m_), paramId(pid), s(s_), v(v_)
		{
		}
	};

	typedef std::map<xstr_t, Srp6aInfo> Srp6aMap;
	typedef std::map<xstr_t, Verifier> VerifierMap;

	enum Section {
		SCT_UNKNOWN,
		SCT_SRP6A,
		SCT_VERIFIER,
	};

	ostk_t *_ostk;
	std::string _filename;
	time_t _mtime;

	Srp6aMap _sMap;
	VerifierMap _vMap;

	void _load();
	void _add_internal_parameters();
	void _add_internal(const char *id, const char *hash, int bits, uintmax_t g, const char *N_str);

	void _add_item(int lineno, Section section, uint8_t *start, uint8_t *end);
	ShadowBox(const std::string& filename);
public:
	static ShadowBoxPtr create();
	static ShadowBoxPtr createFromFile(const std::string& filename);
	virtual ~ShadowBox();

	ShadowBoxPtr reload();

	std::string filename() const		{ return _filename; }

	void dump(xio_write_function write, void *cookie);

	bool empty() const 			{ return _vMap.empty(); }

	size_t count() const			{ return _vMap.size(); }

	bool getVerifier(const xstr_t& identity, xstr_t& method, xstr_t& paramId, xstr_t& salt, xstr_t& verifier);

	bool getSrp6aParameter(const xstr_t& paramId, xstr_t& hashId, int& bits, uintmax_t& g, xstr_t& N);

	Srp6aServerPtr newSrp6aServer(const xstr_t& paramId);
};


#endif
