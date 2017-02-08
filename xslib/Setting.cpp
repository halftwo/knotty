#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "Setting.h"
#include "ScopeGuard.h"
#include "binary_prefix.h"
#include "xstr.h"
#include <stdio.h>
#include <string.h>
#include <map>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: Setting.cpp,v 1.12 2012/12/24 07:06:21 gremlin Exp $";
#endif

class SettingI: public Setting
{
public:
	SettingI();
	virtual ~SettingI();

	virtual xstr_t getXstr(const std::string& name, const xstr_t& dft = xstr_null);

	virtual std::string getString(const std::string& name, const std::string& dft = std::string());
	virtual intmax_t getInt(const std::string& name, intmax_t dft = 0);
	virtual bool getBool(const std::string& name, bool dft = false);
	virtual double getReal(const std::string& name, double dft = 0.0);

	virtual std::vector<std::string> getStringList(const std::string& name);

	virtual std::string wantString(const std::string& name);
	virtual intmax_t wantInt(const std::string& name);
	virtual bool wantBool(const std::string& name);
	virtual double wantReal(const std::string& name);

	virtual std::vector<std::string> wantStringList(const std::string& name);

	virtual void set(const std::string& name, const std::string& value);
	virtual bool insert(const std::string& name, const std::string& value);
	virtual bool update(const std::string& name, const std::string& value);

	virtual void load(const std::string& file);
	virtual SettingPtr clone();

private:
	SettingI(SettingI*);
	SettingI(const SettingI&);
	SettingI& operator=(const SettingI&);

	std::string* _find(const std::string& key);

	typedef std::map<std::string, std::string> MapType;

	MapType _map;
};

SettingPtr newSetting()
{
	return SettingPtr(new SettingI());
}

SettingPtr loadSetting(const std::string& file)
{
	SettingPtr setting(new SettingI);
	setting->load(file);
	return setting;
}

SettingI::SettingI()
{
}

SettingI::~SettingI()
{
}

SettingI::SettingI(SettingI *st)
{
	_map = st->_map;
}

SettingPtr SettingI::clone()
{
	return SettingPtr(new SettingI(this));
}

void SettingI::load(const std::string& file)
{
	FILE *fp = fopen(file.c_str(), "rb");
	if (!fp)
		throw XERROR_FMT(XError, "failed to open file %s", file.c_str());
	ON_BLOCK_EXIT(fclose, fp);
	
	char *line = NULL;
	size_t size = 0;
	ON_BLOCK_EXIT(free_pptr<char>, &line);

	ssize_t n;
	while ((n = getline(&line, &size, fp)) > 0)
	{
		xstr_t v = XSTR_INIT((unsigned char*)line, n);
		xstr_trim(&v);

		if (v.len == 0 || v.data[0] == '#')
			continue;

		xstr_t k;
		if (!xstr_delimit_char(&v, '=', &k))
			continue;

		xstr_rtrim(&k);
		xstr_ltrim(&v);
		if (k.len == 0 || v.len == 0)
			continue;

		_map[make_string(k)] = make_string(v);
	}
}

std::string* SettingI::_find(const std::string& key)
{
	MapType::iterator iter = _map.find(key);
	if (iter != _map.end())
		return &iter->second;
	return NULL;
}

xstr_t SettingI::getXstr(const std::string& name, const xstr_t& dft)
{
	std::string *v = _find(name);
	if (v)
	{
		xstr_t xs = XSTR_CXX(*v);
		return xs;
	}
	return dft;
}

std::string SettingI::wantString(const std::string& name)
{
	std::string *v = _find(name);
	if (!v)
		throw XERROR_MSG(SettingItemMissingError, name);
	if (v->empty())
		throw XERROR_MSG(SettingItemEmptyError, name);
	return *v;
}

std::string SettingI::getString(const std::string& name, const ::std::string& dft)
{
	std::string *v = _find(name);
	if (v)
	{
		return *v;
	}
	return dft;
}

static bool _parseInt(const std::string& s, intmax_t& v)
{
	char *end;
	const char *start = s.c_str();
	intmax_t i = strtoll(start, &end, 0);
	if (end > start)
	{
		if (end[0])
		{
			intmax_t x = binary_prefix(end, &end);
			if (end[0] == 0)
			{
				i *= x;
			}
			else
			{
				double r = strtod(start, &end);
				if (end > start && end[0] == 0)
				{
					i = r > INTMAX_MAX ? INTMAX_MAX
						 : r < INTMAX_MIN ? INTMAX_MIN
						 : (intmax_t)r;
				}
				else
					goto invalid;
			}
		}
		v = i;
		return true;
	}
invalid:
	return false;
}

intmax_t SettingI::wantInt(const std::string& name)
{
	std::string s = wantString(name);
	intmax_t i;
	if (!_parseInt(s, i))
		throw XERROR_MSG(SettingItemSyntaxError, name);
	return i;
}

intmax_t SettingI::getInt(const std::string& name, intmax_t dft)
{
	std::string *v = _find(name);
	if (v)
	{
		intmax_t i;
		if (_parseInt(*v, i))
			return i;
	}
	return dft;
}

static bool _parseBool(const std::string& s, bool& v)
{
	const char *str = s.c_str();
	if (isdigit(str[0]) || str[0] == '-' || str[0] == '+')
	{
		v = atoi(str);
		return true;
	}

	if (strcasecmp(str, "true") == 0 || strcasecmp(str, "yes") == 0
		|| strcasecmp(str, "t") == 0 || strcasecmp(str, "y") == 0)
	{
		v = true;
		return true;
	}

	if (strcasecmp(str, "false") == 0 || strcasecmp(str, "no") == 0
		|| strcasecmp(str, "f") == 0 || strcasecmp(str, "n") == 0)
	{
		v = false;
		return true;
	}

	return false;
}

bool SettingI::wantBool(const std::string& name)
{
	std::string s = wantString(name);
	bool t;
	if (!_parseBool(s, t))
		throw XERROR_MSG(SettingItemSyntaxError, name);
	return t;
}

bool SettingI::getBool(const std::string& name, bool dft)
{
	std::string *v = _find(name);
	if (v)
	{
		bool t;
		if (_parseBool(*v, t))
			return t;
	}
	return dft;
}

static bool _parseReal(const std::string& s, double& v)
{
	char *end;
	const char *start = s.c_str();
	double r = strtod(start, &end);
	if (end > start)
	{
		if (end[0])
		{
			intmax_t x = binary_prefix(end, &end);
			if (end[0] == 0)
			{
				r *= x;
			}
			else
				goto invalid;
		}
		v = r;
		return true;
	}
invalid:
	return false;
}

double SettingI::wantReal(const std::string& name)
{
	std::string s = wantString(name);
	double r;
	if (!_parseReal(s, r))
		throw XERROR_MSG(SettingItemSyntaxError, name);
	return r;
}

double SettingI::getReal(const std::string& name, double dft)
{
	std::string *v = _find(name);
	if (v)
	{
		double r;
		if (_parseReal(*v, r))
			return r;
	}
	return dft;
}

static xstr_t _delimit_xs = XSTR_CONST(", \t\r\n");

std::vector<std::string> SettingI::wantStringList(const std::string& name)
{
	std::string v = wantString(name);
	std::vector<std::string> result;
	xstr_t xs = XSTR_CXX(v);
	xstr_t s;
	while (xstr_token(&xs, &_delimit_xs, &s))
	{
		result.push_back(make_string(s));
	}
	return result;
}

std::vector<std::string> SettingI::getStringList(const std::string& name)
{
	std::vector<std::string> result;
	std::string *v = _find(name);
	if (v)
	{
		xstr_t xs = XSTR_CXX(*v);
		xstr_t s;
		while (xstr_token(&xs, &_delimit_xs, &s))
		{
			result.push_back(make_string(s));
		}
	}
	return result;
}

void SettingI::set(const std::string& name, const std::string& value)
{
	_map[name] = value;
}

bool SettingI::insert(const std::string& name, const std::string& value)
{
	MapType::iterator iter = _map.find(name);
	if (iter == _map.end())
	{
		_map[name] = value;
		return true;
	}

	return false;
}

bool SettingI::update(const std::string& name, const std::string& value)
{
	MapType::iterator iter = _map.find(name);
	if (iter != _map.end())
	{
		iter->second = value;
		return true;
	}

	return false;
}

