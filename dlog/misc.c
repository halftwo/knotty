#include "misc.h"
#include <time.h>
#include <stdio.h>

char *get_time_str(time_t t, char buf[])
{
	struct tm tm;

	localtime_r(&t, &tm);
	sprintf(buf, "%02d%02d%02d-%02d%02d%02d",
		tm.tm_year < 100 ? tm.tm_year : tm.tm_year - 100, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
	return buf;
}

