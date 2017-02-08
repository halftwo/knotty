#ifndef Context_h_
#define Context_h_

#include "VData.h"
#include "xslib/XRefCount.h"
#include "xslib/vbs_pack.h"
#include "xslib/ostk.h"


namespace xic
{

class Quest;
class Context;
typedef XPtr<Context> ContextPtr;


class Context: virtual public XRefCount
{
public:
	bool empty() const 			{ return (_dict->count == 0); }
	const vbs_dict_t *dict() const 		{ return _dict; }

	intmax_t getInt(const char *name, intmax_t dft = 0) const;
	bool getBool(const char *name, bool dft = false) const;
	double getFloating(const char *name, double dft = 0.0) const;
	decimal64_t getDecimal64(const char *name, decimal64_t dft = decimal64_zero) const;
	std::string getString(const char *name, const std::string& dft = std::string()) const;
	xstr_t getXstr(const char *name, const xstr_t& dft = xstr_null) const;

private:
	friend class ContextBuilder;
	Context(ostk_t *ostk, vbs_dict_t *dict);
	Context(const Context& r);
	Context& operator=(const Context& r);
	virtual ~Context();
	virtual void xref_destroy();

	vbs_data_t *_find(const char *name) const;

	ostk_t *_ostk;
	vbs_dict_t *_dict;
};


class ContextBuilder
{
	ContextBuilder(const ContextBuilder& r);
	ContextBuilder& operator=(const ContextBuilder& r);
public:
	ContextBuilder();
	~ContextBuilder();

	explicit ContextBuilder(const VDict& d);

	template<typename VALUE>
	ContextBuilder(const char *name, const VALUE& value)
	{
		_init();
		set(name, value);
	}

	template<typename VALUE>
	ContextBuilder& operator()(const char *name, const VALUE& value)
	{
		set(name, value);
		return *this;
	}

	ContextPtr build();

	void set(const char *name, const xstr_t& v);
	void set(const char *name, const std::string& v);
	void set(const char *name, const char *v);
	void set(const char *name, const char *data, size_t size);
	void set(const char *name, bool v);
	void set(const char *name, double v);
	void set(const char *name, decimal64_t v);

	void set(const char *name, int v)		{ _seti(name, v); }
	void set(const char *name, long v)		{ _seti(name, v); }
	void set(const char *name, long long v)		{ _seti(name, v); }

private:
	void *operator new(size_t size);
	void _init();

	vbs_ditem_t *_put_item(const char *name);
	void _seti(const char *name, intmax_t v);

	ostk_t *_ostk;
	vbs_dict_t *_dict;
};


class ContextPacker
{
public:
	ContextPacker(Quest* q);
	~ContextPacker();
	void init();
	void finish();

	void pack(const char *name, const xstr_t& v);
	void pack(const char *name, const std::string& v);
	void pack(const char *name, const char *v);
	void pack(const char *name, const char *data, size_t size);
	void pack(const char *name, bool v);
	void pack(const char *name, double v);
	void pack(const char *name, decimal64_t v);

	void pack(const char *name, int v)		{ _packi(name, v); }
	void pack(const char *name, long v)		{ _packi(name, v); }
	void pack(const char *name, long long v)	{ _packi(name, v); }

	void pack(const char *name, unsigned int v)	{ _packu(name, v); }
	void pack(const char *name, unsigned long v)	{ _packu(name, v); }
	void pack(const char *name, unsigned long long v) { _packu(name, v); }

private:
	void *operator new(size_t size);
	void _packi(const char *name, intmax_t v);
	void _packu(const char *name, uintmax_t v);

	Quest *_q;
	vbs_packer_t _pk;
};


};

#endif

