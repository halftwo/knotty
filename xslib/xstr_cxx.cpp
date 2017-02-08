#include "xstr.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xstr_cxx.cpp,v 1.2 2012/01/30 06:55:30 jiagui Exp $";
#endif

std::string make_string_lower(const xstr_t& xs)
{
	std::string s((const char *)xs.data, xs.len);
	for (size_t i = xs.len; i--; )
	{
		int ch = (unsigned char)xs.data[i];
		if (ch >= 'A' && ch <= 'Z')
			s[i] = (ch | 0x20);
	}
	return s;
}

std::string make_string_upper(const xstr_t& xs)
{
	std::string s((const char *)xs.data, xs.len);
	for (size_t i = xs.len; i--; )
	{
		int ch = (unsigned char)xs.data[i];
		if (ch >= 'a' && ch <= 'z')
			s[i] = (ch & ~0x20);
	}
	return s;
}

std::string operator+(const xstr_t& xs1, const xstr_t& xs2)
{
	std::string s;
	s.reserve(xs1.len + xs2.len);
	s.append((const char *)xs1.data, xs1.len);
	s.append((const char *)xs2.data, xs2.len);
	return s;
}

std::string operator+(const xstr_t& xs, const std::string& str)
{
	std::string s;
	s.reserve(xs.len + str.length());
	s.append((const char *)xs.data, xs.len);
	s.append(str);
	return s;
}

std::string operator+(const std::string& str, const xstr_t& xs)
{
	std::string s;
	s.reserve(str.length() + xs.len);
	s.append(str);
	s.append((const char *)xs.data, xs.len);
	return s;
}

std::string operator+(const xstr_t& xs, const char *str)
{
	size_t len = strlen(str);
	std::string s;
	s.reserve(xs.len + len);
	s.append((const char *)xs.data, xs.len);
	s.append(str, len);
	return s;
}

std::string operator+(const char *str, const xstr_t& xs)
{
	size_t len = strlen(str);
	std::string s;
	s.reserve(len + xs.len);
	s.append(str, len);
	s.append((const char *)xs.data, xs.len);
	return s;
}

