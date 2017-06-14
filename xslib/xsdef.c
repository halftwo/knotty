#include "xsdef.h"

const char *xslib_version_string()
{
	return XS_TOSTR(XSLIB_VERSION);
}

const char *xslib_version_rcsid()
{
	return "$xslib: " XS_TOSTR(XSLIB_VERSION) " $";
}

