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
	struct Srp6aParameter
	{
		int bits;
		uintmax_t g;
		xstr_t N;
		Srp6aParameter(int b_, uintmax_t g_, const xstr_t& N_)
			: bits(b_), g(g_), N(N_)
		{
		}
	};

	struct SecretVerifier
	{
		xstr_t method;
		xstr_t paramId;
		xstr_t hashId;
		xstr_t salt;
		xstr_t verifier;

		SecretVerifier(const xstr_t& m_, const xstr_t& pid, const xstr_t& h_, const xstr_t& s_, const xstr_t& v_)
			: method(m_), paramId(pid), hashId(h_), salt(s_), verifier(v_)
		{
		}
	};

	typedef std::map<xstr_t, Srp6aParameter> Srp6aParameterMap;
	typedef std::map<xstr_t, SecretVerifier> SecretVerifierMap;

	enum Section {
		SCT_UNKNOWN,
		SCT_SRP6A,
		SCT_VERIFIER,
	};

	ostk_t *_ostk;
	std::string _filename;
	time_t _mtime;

	Srp6aParameterMap _sMap;
	SecretVerifierMap _vMap;

	void _load();
	void _add_internal_parameters();
	void _add_internal(const char *id, int bits, uintmax_t g, const char *N_str);

	void _add_item(int lineno, Section section, uint8_t *start, uint8_t *end);
	
	Srp6aParameter *_getSrp6aParameter(const xstr_t& paramId);

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

	bool getVerifier(const xstr_t& identity, xstr_t& method, xstr_t& paramId, 
				xstr_t& hashId, xstr_t& salt, xstr_t& verifier);

	bool getSrp6aParameter(const xstr_t& paramId, int& bits, uintmax_t& g, xstr_t& N);

	Srp6aServerPtr newSrp6aServer(const xstr_t& paramId, const xstr_t& hashId);
};


#endif
