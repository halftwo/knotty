#include "httpcode.h"
#include "xsdef.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: httpcode.c,v 1.3 2012/09/20 03:21:47 jiagui Exp $";
#endif

struct code_desc_struct
{
	int code;
	const char *desc;
};

struct code_desc_struct _cds[] =
{
#define HC(code, name, desc)	{ code, desc },
	HTTPCODES
#undef HC
};

const char *httpcode_description(int code)
{
	int i;
	for (i = 0; i < XS_ARRCOUNT(_cds); ++i)
	{
		if (code == _cds[i].code)
			return _cds[i].desc;
	}
	return "";
}

