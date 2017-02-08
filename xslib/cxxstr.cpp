#include "cxxstr.h"
#include "xformat.h"
#include <cxxabi.h>
#include <sstream>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: cxxstr.cpp,v 1.9 2013/09/12 07:29:19 gremlin Exp jiagui $";
#endif

std::string vformat_string(const char *fmt, va_list ap)
{
	char buf[256];
	std::ostringstream ss;
	vxformat(NULL, ostream_xio.write, (std::ostream*)&ss, buf, sizeof(buf), fmt, ap);
	return ss.str();
}

std::string format_string(const char *fmt, ...)
{
	std::ostringstream ss;
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vxformat(NULL, ostream_xio.write, (std::ostream*)&ss, buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return ss.str();
}

std::string vxformat_string(xfmt_callback_function callback, const char *fmt, va_list ap)
{
	char buf[256];
	std::ostringstream ss;
	vxformat(callback, ostream_xio.write, (std::ostream*)&ss, buf, sizeof(buf), fmt, ap);
	return ss.str();
}

std::string xformat_string(xfmt_callback_function callback, const char *fmt, ...)
{
	std::ostringstream ss;
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vxformat(callback, ostream_xio.write, (std::ostream*)&ss, buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return ss.str();
}

ssize_t ostream_vprintf(std::ostream& os, const char *fmt, va_list ap)
{
	char buf[256];
	return vxformat(NULL, ostream_xio.write, (std::ostream*)&os, buf, sizeof(buf), fmt, ap);
}

ssize_t ostream_printf(std::ostream& os, const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	ssize_t rc = vxformat(NULL, ostream_xio.write, (std::ostream*)&os, buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return rc;
}

ssize_t ostream_vxprintf(std::ostream& os, xfmt_callback_function callback, const char *fmt, va_list ap)
{
	char buf[256];
	return vxformat(callback, ostream_xio.write, (std::ostream*)&os, buf, sizeof(buf), fmt, ap);
}

ssize_t ostream_xprintf(std::ostream& os, xfmt_callback_function callback, const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	ssize_t rc = vxformat(callback, ostream_xio.write, (std::ostream*)&os, buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return rc;
}

std::string demangle_cxxname(const char *mangled)
{
	std::string r;
	int status = 0;
	char *demangled = abi::__cxa_demangle(mangled, NULL, 0, &status);
	if (demangled)
	{
		r = std::string(demangled);
		free(demangled);
	}
	else
	{
		r = std::string(mangled);
	}
	return r;
}

char *demangle_cxxname(const char *mangled, char *buffer, size_t *length)
{
	int status;
	return abi::__cxa_demangle(mangled, buffer, length, &status);
}

