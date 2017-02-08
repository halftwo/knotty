/* $Id: os_win.h,v 1.1 2014/01/26 09:01:52 gremlin Exp $ */
#ifndef OS_WIN_H_
#define OS_WIN_H_

#ifdef WIN32

#include <basetsd.h>

typedef SSIZE_T ssize_t;

#endif /* WIN32 */


#ifdef _MSC_VER

#ifndef __cplusplus
# define inline __inline
#endif

#endif /* _MSC_VER */


#endif
