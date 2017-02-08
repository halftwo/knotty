#include "httpmisc.h"
#include <stdio.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: httpmisc.c,v 1.1 2011/09/29 07:50:30 jiagui Exp $";
#endif

static char *_wdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *_months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* Tue, 09 Jun 2009 08:59:50 GMT */

size_t httpdate_format(char buf[], size_t size, time_t t) 
{
	struct tm tm;
	gmtime_r(&t, &tm);
	return snprintf(buf, size, "%s, %02d %s %d %02d:%02d:%02d GMT",
		_wdays[tm.tm_wday], tm.tm_mday, _months[tm.tm_mon], tm.tm_year + 1900,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
}

