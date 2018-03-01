/*
   Author: XIONG Jiagui
   Date: 2007-03-26
 */
#include "XError.h"
#include "XLock.h"
#include "cxxstr.h"
#include "xformat.h"
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <cxxabi.h>
#include <typeinfo>
#include <sstream>

#if defined(__linux) && (defined(__i386) || defined(__x86_64))
#include <execinfo.h>
#define HAVE_BACKTRACE
#endif

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: XError.cpp,v 1.37 2015/04/08 08:43:10 gremlin Exp $";
#endif

#define OUTER_SKIP	2
#define INNER_SKIP	1

static XRecMutex what_mutex;

int XError::how = 1;


XCallTrace::Data::Data()
{
	OREF_INIT(this);
	_done = false;
	_num = 0;
	_how = 0;
}

const std::string& XCallTrace::Data::str() throw()
{
	if (!_done)
	{
		XRecMutex::Lock lock(what_mutex);
		if (!_done)
		{
			int n = _num - INNER_SKIP - OUTER_SKIP;
			if (n <= 0)
				goto done;

			char **symbols = NULL;
#ifdef HAVE_BACKTRACE
			symbols = backtrace_symbols(_stk + INNER_SKIP, n);
#endif
			if (!symbols)
				goto done;

			char *de_buf = NULL;
			size_t de_size = 0;
			std::ostringstream os;

			try
			{
				for (int i = 0; i < n; ++i)
				{
					/* Format:
						./a.out(_Z3fooRKSs+0x55) [0x402b9d]
					 */
					char *line = symbols[i];
					char *func = NULL;
					char *parentheses = strchr(line, '(');
					if (parentheses)
					{
						char *p0 = parentheses + 1;
						char *p1 = strchr(p0, ')');
						if (p1)
						{
							char *plus = (char *)memrchr(p0, '+', p1 - p0);
							if (plus)
							{
								*plus = 0;
								func = p0;
							}
						}
					}

					if (i)
						os << '\n';

					if (func)
					{
						os.write(line, parentheses - line) << ' ';

						char *demangled = NULL;
						if (_how > 1)
						{
							int status = 0;
							demangled = abi::__cxa_demangle(func, de_buf, &de_size, &status);
							if (demangled && demangled != de_buf)
								de_buf = demangled;
						}

						os << (demangled ? demangled : func);
					}
					else
					{
						os << line;
					}
				}
			}
			catch (...)
			{
			}

			if (de_buf)
				free(de_buf);
			free(symbols);
			_str = os.str();
		}
	done:
		_done = true;
	}
	return _str;
}

XCallTrace::XCallTrace()
{
	_dat = new Data();
	_dat->_how = INT_MAX;
	_dat->_num = 0;
#ifdef HAVE_BACKTRACE
	_dat->_num = backtrace(_dat->_stk, sizeof(_dat->_stk) / sizeof(_dat->_stk[0]));
#endif
}

XCallTrace::XCallTrace(int how)
{
	if (how)
	{
		_dat = new Data();
		_dat->_how = how;
		_dat->_num = 0;
#ifdef HAVE_BACKTRACE
		_dat->_num = backtrace(_dat->_stk, sizeof(_dat->_stk) / sizeof(_dat->_stk[0]));
#endif
	}
	else
	{
		_dat = NULL;
	}
}

XCallTrace& XCallTrace::operator=(const XCallTrace& r)
{
	if (_dat != r._dat)
	{
		if (r._dat) OREF_INC(r._dat);
		if (_dat) OREF_DEC(_dat, Data::destroy);
		_dat = r._dat;
	}
	return *this;
}

const std::string& XCallTrace::str() const throw()
{
	static std::string _empty_string;
	return _dat ? _dat->str() : _empty_string;
}


const XRecMutex& XError::_mutex()
{
	return what_mutex;
}

const char *XError::what() const throw()
{
	XRecMutex::Lock lock(_mutex());
	if (_what.empty())
	{
		try
		{
			std::ostringstream os;

			os << exname() << '(' << _code;
			if (_tag[0])
				os << ',' << _tag;
			os << ')';

			if (_file && _file[0])
				os << " at " << _file << ':' << _line;
			if (!_message.empty())
				os << " --- " << _message;

			_what = os.str();
		}
		catch (...)
		{
		}
	}
	return _what.c_str();
}

std::string XError::format(const char *fmt, ...)
{
	std::string s;

	if (fmt)
	{
		va_list ap;
		va_start(ap, fmt);
		s = vformat_string(fmt, ap);
		va_end(ap);
	}
	else
	{
		s = "BUG! BUG! BUG! fmt string is NULL in XError::format().";
	}

	return s;
}


#ifdef TEST_XERROR

#include <iostream>

void throw_and_catch(const XError& ex)
{
	try 
	{
		std::cerr << "----------------------------------" << std::endl;
		// NOTE: "throw ex" will throw a XError
		// it's not the same as ex.do_throw()
		ex.do_throw();
	}
	catch (XError& e)
	{
		std::cerr << e.exname() << std::endl;
		std::cerr << e.what() << std::endl;
		std::cerr << e.calltrace() << std::endl;
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

XE_X(XError, MyException, "myapp.MyException");

int main()
{
	std::cerr << "sizeof(XError) = " << sizeof(XError) << std::endl;

	{
		throw_and_catch(XERROR(MyException));
	}

	{
		// Should be: XERROR_VAR_CODE_FMT(MyException, ex, 0, "hi");
		XERROR_VAR_FMT(MyException, ex, 0, "hi");
		throw_and_catch(ex);
	}

	{
		XERROR_VAR_TAG_FMT(MyException, ex, "server_error", "hi");
		throw_and_catch(ex);
	}

	return 0;
}

#endif

