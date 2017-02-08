#ifndef nvdlog_h_
#define nvdlog_h_

#include "dlog.h"
#include <sstream>
#include <vector>

class Nvdlog
{
	enum { TRUNCATE_SIZE = 256, };
public:
	Nvdlog(const char* tag, const char* locus)
		: _first(true), _identity(0), _tag(tag), _locus(locus)
	{}

	Nvdlog(const char* identity, const char* tag, const char* locus)
		: _first(true), _identity(identity), _tag(tag), _locus(locus)
	{}

	~Nvdlog()
	{
		uxdlog(NULL, _identity, _tag, _locus, "%s", _ss.str().c_str());
	}

	Nvdlog& operator()(const char *name, const std::string& value)
	{
		if (_first)
			_first = false;
		else
			_ss << " ";

		_ss << name << "=";
		if (value.length() > TRUNCATE_SIZE)
			_ss << value.substr(0, TRUNCATE_SIZE) << "..." << value.size();
		else
			_ss << value;
		return *this;
	}

	template <class ValueType>
	Nvdlog& operator()(const char *name, const ValueType& value)
	{
		if (_first)
			_first = false;
		else
			_ss << " ";

		_ss << name << "=" << value;
		return *this;
	}

	template <class ValueType>
	Nvdlog& operator()(const char *name, const std::vector<ValueType>& value, const char *sep = ",")
	{
		if (_first)
			_first = false;
		else
			_ss << " ";

		int num = value.size();
		_ss << name << "=[" << num;

		int i;
		long pos = _ss.tellp();
		pos += TRUNCATE_SIZE;
		for (i = 0; i < num; ++i)
		{
			if (i == 0)
				_ss << ":" << value[i];
			else
				_ss << sep << value[i];

			if (_ss.tellp() > pos)
				break;
		}
		if (i < num)
			_ss << sep << "...]";
		else
			_ss << "]";
		
		return *this;
	}

	Nvdlog& operator()(const std::string& name, const std::string& value)
	{
		return operator()(name.c_str(), value);
	}

	template <class ValueType>
	Nvdlog& operator()(const std::string& name, const ValueType& value)
	{
		return operator()(name.c_str(), value);
	}

	template <class ValueType>
	Nvdlog& operator()(const std::string& name, const std::vector<ValueType>& value, const std::string& sep = ",")
	{
		return operator()(name.c_str(), value, sep.c_str());
	}

private:
	void *operator new(size_t);
	void operator delete(void *p);
	Nvdlog(const Nvdlog&);
	Nvdlog& operator=(const Nvdlog&);

	bool _first;
	const char* _identity;
	const char* _tag;
	const char* _locus;
	std::ostringstream _ss;
};


#define nvdlog(tag)	Nvdlog(tag, __FILE__":"_dLoG_sTr__(__LINE__))

#endif
