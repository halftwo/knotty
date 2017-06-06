/* $Id: XError.h,v 1.42 2015/04/04 13:44:06 gremlin Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2007-03-26
 */
#ifndef XError_h_
#define XError_h_

#include "xsdef.h"
#include "oref.h"
#include <string>
#include <stdexcept>


#define XASSERT(expr)						\
		((expr)	? (void)0				\
		: throw XERROR_MSG(XAssertError, 		\
			"`" XS_TOSTR(expr) "` failed"), (void)0)


#define XERROR(XErr)						\
		XErr(__FILE__, __LINE__)

#define XERROR_MSG(XErr, message)				\
		XErr(__FILE__, __LINE__, 0, "", (message))

#define XERROR_FMT(XErr, ...)					\
		XErr(__FILE__, __LINE__, 0, "", XError::format(__VA_ARGS__))

#define XERROR_CODE(XErr, code)					\
		XErr(__FILE__, __LINE__, (code))

#define XERROR_CODE_MSG(XErr, code, message)			\
		XErr(__FILE__, __LINE__, (code), "", (message))

#define XERROR_CODE_FMT(XErr, code, ...)			\
		XErr(__FILE__, __LINE__, (code), "", XError::format(__VA_ARGS__))

#define XERROR_TAG(XErr, tag)					\
		XErr(__FILE__, __LINE__, 0, (tag))

#define XERROR_TAG_MSG(XErr, tag, message)			\
		XErr(__FILE__, __LINE__, 0, (tag), (message))

#define XERROR_TAG_FMT(XErr, tag, ...)				\
		XErr(__FILE__, __LINE__, 0, (tag), XError::format(__VA_ARGS__))


#define XERROR_VAR(XErr, ex)					\
		XErr ex(__FILE__, __LINE__)

#define XERROR_VAR_MSG(XErr, ex, message)			\
		XErr ex(__FILE__, __LINE__, 0, "", (message))

#define XERROR_VAR_FMT(XErr, ex, ...)				\
		XErr ex(__FILE__, __LINE__, 0, "", XError::format(__VA_ARGS__))

#define XERROR_VAR_CODE(XErr, ex, code)				\
		XErr ex(__FILE__, __LINE__, (code))

#define XERROR_VAR_CODE_MSG(XErr, ex, code, message)		\
		XErr ex(__FILE__, __LINE__, (code), "", (message))

#define XERROR_VAR_CODE_FMT(XErr, ex, code, ...)		\
		XErr ex(__FILE__, __LINE__, (code), "", XError::format(__VA_ARGS__))

#define XERROR_VAR_TAG(XErr, ex, tag)				\
		XErr ex(__FILE__, __LINE__, 0, (tag))

#define XERROR_VAR_TAG_MSG(XErr, ex, tag, message)		\
		XErr ex(__FILE__, __LINE__, 0, (tag), (message))

#define XERROR_VAR_TAG_FMT(XErr, ex, tag, ...)			\
		XErr ex(__FILE__, __LINE__, 0, (tag), XError::format(__VA_ARGS__))


class XCallTrace
{
	struct Data
	{
		OREF_DECLARE();
		std::string _str;
		short _done;
		short _num;
		int _how;
		void *_stk[60];

		Data();
		const std::string& str() noexcept;
		static void destroy(Data *dat)	{ delete dat; }
	} *_dat;
public:
	XCallTrace();
	XCallTrace(int how);
	XCallTrace(const XCallTrace& r)		: _dat(r._dat) { if (_dat) OREF_INC(_dat); }
	XCallTrace& operator=(const XCallTrace& r);
	~XCallTrace()				{ if (_dat) OREF_DEC(_dat, Data::destroy); }

	const std::string& str() const noexcept;
};


class XRecMutex;

/*
  If XError::how greater than 0, XError::calltrace() will return a string
  representing the stack of the function calls at the time that the XError
  is thrown.  One function at a line.
  If XError::how is greater than 1, the C++ function names in the calltrace()
  are demangled.  Otherwise, the C++ function names in the calltrace() are 
  not demangled.  
  The default value of XError::how is 1.
  To make the calltrace information human understandable, compile the source
  file with -rdynamic option.
	  gcc -rdynamic
	  g++ -rdynamic
 */

class XError: public std::exception
{
public:
	XError(const char *file, int line, int code = 0,
			const std::string& tag = "",
			const std::string& msg = "", 
			const XCallTrace& ct = XCallTrace(XError::how))
		: _file(file), _line(line), _code(code), _tag(tag),
		  _message(msg), _calltrace(ct)
	{}

	virtual ~XError() noexcept 			{}

	virtual XError* clone() const			{ return new XError(*this); }
	virtual void do_throw() const			{ throw *this; }
	virtual const char* exname() const noexcept	{ return "XError"; }

	virtual const char* what() const noexcept;

	const char* file() const noexcept 		{ return _file; }
	int line() const noexcept 			{ return _line; }
	int code() const noexcept 			{ return _code; }
	const std::string& tag() const noexcept 	{ return _tag; }
	const std::string& message() const noexcept 	{ return _message; }
	const std::string& calltrace() const noexcept	{ return _calltrace.str(); }

	static std::string format(const char *fmt, ...) XS_C_PRINTF(1, 2);
	static int how;

protected:
	static const XRecMutex& _mutex();

	const char* const _file;
	int _line;
	int _code;
	std::string _tag;
	std::string _message;
	XCallTrace _calltrace;
	mutable std::string _what;	// Initialized lazily in what().
};


#define XE_DEFAULT_METHODS_WITHOUT_EXNAME(BASE, DERIVED)		\
	DERIVED(const char *file, int line, int code = 0,		\
		const std::string& tag = "", const std::string& msg = "",\
		const XCallTrace& ct = XCallTrace(XError::how)		\
		) : BASE(file, line, code, tag, msg, ct) {}		\
	virtual DERIVED* clone() const		{ return new DERIVED(*this); }\
	virtual void do_throw() const		{ throw *this; }	\


#define XE_DEFAULT_METHODS_EX(BASE, DERIVED, EXNAME)			\
	XE_DEFAULT_METHODS_WITHOUT_EXNAME(BASE, DERIVED)		\
	virtual const char* exname() const noexcept { return "" EXNAME; }


#define XE_DEFAULT_METHODS(BASE, DERIVED)				\
	XE_DEFAULT_METHODS_EX(BASE, DERIVED, XS_TOSTR(DERIVED))


#define XE_X(BASE, DERIVED, EXNAME)					\
	class DERIVED: public BASE {					\
	public:								\
		XE_DEFAULT_METHODS_EX(BASE, DERIVED, EXNAME)		\
	};


#define XE_(BASE, DERIVED)	XE_X(BASE, DERIVED, XS_TOSTR(DERIVED))



XE_(XError, 		XLogicError)
XE_(XLogicError, 		XAssertError)
XE_(XLogicError, 		XNullPointerError)
XE_(XLogicError, 		XOutRangeError)
XE_(XLogicError, 		XArgumentError)

XE_(XError, 		XSyscallError)
XE_(XError, 		XMemoryError)
XE_(XError, 		XUnsupportedError)


#endif
