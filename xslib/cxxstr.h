/* $Id: cxxstr.h,v 1.9 2013/09/12 07:29:19 gremlin Exp jiagui $ */
#ifndef CXXSTR_H_
#define CXXSTR_H_

#include "xsdef.h"
#include "xformat.h"
#include <stdarg.h>
#include <string>
#include <iostream>


std::string format_string(const char *fmt, ...) XS_C_PRINTF(1, 2);


std::string xformat_string(xfmt_callback_function callback/*NULL*/,
			const char *fmt, ...) XS_C_PRINTF(2, 3);


std::string vformat_string(const char *fmt, va_list ap) XS_C_PRINTF(1, 0);

std::string vxformat_string(xfmt_callback_function callback/*NULL*/,
			const char *fmt, va_list ap) XS_C_PRINTF(2, 0);


ssize_t ostream_printf(std::ostream& os,
			const char *fmt, ...) XS_C_PRINTF(2, 3);

ssize_t ostream_xprintf(std::ostream& os, xfmt_callback_function callback/*NULL*/,
			const char *fmt, ...) XS_C_PRINTF(3, 4);


ssize_t ostream_vprintf(std::ostream& os,
			const char *fmt, va_list ap) XS_C_PRINTF(2, 0);

ssize_t ostream_vxprintf(std::ostream& os, xfmt_callback_function callback/*NULL*/,
			const char *fmt, va_list ap) XS_C_PRINTF(3, 0);


std::string demangle_cxxname(const char *mangled);

/*
   'buffer' must be malloc()ed or NULL.
   The return value should be free()ed by the caller.
 */
char *demangle_cxxname(const char *mangled, char *buffer, size_t *length);


#endif
