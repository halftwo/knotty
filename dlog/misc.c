#include "misc.h"
#include <time.h>
#include <stdio.h>

char *get_time_str(time_t t, bool local, char buf[])
{
	struct tm tm;

	if (local) {
		localtime_r(&t, &tm);
	} else {
		gmtime_r(&t, &tm);
	}

	sprintf(buf, "%02d%02d%02d%c%02d%02d%02d",
		tm.tm_year < 100 ? tm.tm_year : tm.tm_year - 100, tm.tm_mon + 1, tm.tm_mday,
		"umtwrfsu"[tm.tm_wday], tm.tm_hour, tm.tm_min, tm.tm_sec);
	return buf;
}

char *get_timezone(char buf[])
{
	if (timezone == 0)
	{
		buf[0] = '0';
		buf[1] = '0';
		buf[2] = 0;
	}
	else
	{
		long t = timezone > 0 ? timezone : -timezone;
		int sec = t % 60;
		t /= 60;
		int min = t % 60;
		t /= 60;

		// positive timezone value is west, negative is east
		int n = sprintf(buf, "%c%02ld", timezone<0?'+':'-', t);
		if (min || sec)
		{
			n += sprintf(buf + n, "%02d", min);
			if (sec)
				n += sprintf(buf + n, "%02d", sec);
		}
	}
	return buf;
}

