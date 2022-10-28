#ifndef XSVER_H_
#define XSVER_H_ 1

#include "xsdef.h"

#define XSLIB_MAJOR	1
#define XSLIB_MINOR	0
#define XSLIB_PATCH	22102813

#define XSLIB_VERSION	XS_TOSTR(XSLIB_MAJOR) "." XS_TOSTR(XSLIB_MINOR) "." XS_TOSTR(XSLIB_PATCH)


#ifdef __cplusplus
extern "C" {
#endif


const char *xslib_version_string();

const char *xslib_version_rcsid();


#ifdef __cplusplus
}
#endif

#endif

