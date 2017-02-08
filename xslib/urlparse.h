/* $Id: urlparse.h,v 1.9 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2007-03-21
 */
#ifndef urlparse_h_
#define urlparse_h_


#include "xstr.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  scheme://user:password@host:port/path?query#fragment
           ~~~~~~~~~~~~~~~~~~~~~~~
                    netloc
 */

struct urlpart
{
	xstr_t scheme;
	xstr_t netloc;
	xstr_t path;
	xstr_t query;
	xstr_t fragment;

	xstr_t user;
	xstr_t password;
	xstr_t host;
	uint16_t port;
};

int urlparse(const xstr_t *url, struct urlpart *part);

int urlunparse(const struct urlpart *part, char *buf, int size);



#ifdef __cplusplus
}
#endif

#endif
