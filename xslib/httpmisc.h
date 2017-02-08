/* $Id: httpmisc.h,v 1.1 2011/09/29 07:50:30 jiagui Exp $ */
#ifndef HTTPMISC_H_
#define HTTPMISC_H_

#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
	Tue, 09 Jun 2009 08:59:50 GMT
 */
size_t httpdate_format(char buf[], size_t size, time_t t);


#ifdef __cplusplus
}
#endif

#endif
