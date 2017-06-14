/* $Id: xsdef.h,v 1.17 2015/07/14 08:42:38 gremlin Exp $ */
#ifndef XSDEF_H_
#define XSDEF_H_ 1

#include <stddef.h>
#include <sys/types.h>		/* for ssize_t */

#if defined(WIN32) || defined(_MSC_VER)
# include "os_win.h"
#endif


#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
#define XS_C_MALLOC		__attribute__((__malloc__))
#else
#define XS_C_MALLOC
#endif

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define XS_C_PRINTF(fmt_idx, arg_idx)	__attribute__((__format__(__printf__, fmt_idx, arg_idx)))
#else
#define XS_C_PRINTF(fmt_idx, arg_idx)
#endif

#if defined(__GNUC__)
#define XS_DEPRECATED		__attribute__((deprecated))
#define XS_UNUSED		__attribute__((unused))
#else
#define XS_DEPRECATED
#define XS_UNUSED
#endif


#define XS_MIN(a, b)		((a) < (b) ? (a) : (b))
#define XS_MAX(a, b)		((a) > (b) ? (a) : (b))
#define XS_ABS(x)		((x) < 0 ? -(x) : (x))
#define XS_CLAMP(x, low, high)	((x) < (low) ? (low) : (x) > (high) ? (high) : (x))

#define XS_SNL(S)		"" S, (sizeof(S)-1)	/* const string and it's length */

#define XS_ARRCOUNT(arr)	(sizeof(arr) / sizeof((arr)[0]))


#define XS__sTrInGiFy_hElPeR__(x)		#x	/* Don't use this macro directly */
#define XS__cOnCaTeNaTe_hElPeR__(s1, s2) 	s1##s2	/* Don't use this macro directly */


#define XS_TOSTR(x)			XS__sTrInGiFy_hElPeR__(x)

#define XS_CONCAT(a, b) 		XS__cOnCaTeNaTe_hElPeR__(a, b)


#define XS_FILE_LINE			__FILE__ ":" XS_TOSTR(__LINE__)

#define XS_ANONYMOUS_VARIABLE(PREFIX) 	XS_CONCAT(PREFIX, __LINE__) XS_UNUSED



#define XS_ALLOC(TYPE, N)		((TYPE*)malloc((N)*sizeof(TYPE)))
#define XS_CALLOC(TYPE, N)		((TYPE*)calloc((N), sizeof(TYPE)))

#define XS_ALLOC_ONE(TYPE)		XS_ALLOC(TYPE, 1)
#define XS_CALLOC_ONE(TYPE)		XS_CALLOC(TYPE, 1)



#ifdef __cplusplus

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS	1
#endif

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS	1
#endif

#if __cplusplus < 201103L
#define noexcept		throw()
#endif

#endif /* __cplusplus */


#ifdef __cplusplus
extern "C" {
#endif

const char *xslib_version_string();
const char *xslib_version_rcsid();

#ifdef __cplusplus
}
#endif

#endif
