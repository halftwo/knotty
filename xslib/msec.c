#include "msec.h"
#include <time.h>
#include <stdlib.h>
#include <pthread.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: msec.c,v 1.5 2011/11/02 16:01:56 jiagui Exp $";
#endif

static pthread_once_t _slack_once = PTHREAD_ONCE_INIT;
static volatile int64_t _slack_mono_msec;
static volatile int64_t _slack_mono_us;
static volatile int64_t _slack_real_delta;


static inline int64_t get_mono_us()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

static inline int64_t get_real_us()
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

static void *msec_thread(void *arg)
{
	int i;
	struct timespec delay = { 0, 0 };

	for (i = 0; 1; ++i)
	{
		int64_t us = get_mono_us();
		_slack_mono_msec = us / 1000;
		_slack_mono_us = us;

		if (i >= 1000)
		{
			_slack_real_delta = get_real_us() - us;
			i = 0;
		}

		// Make nanosleep() wakes up at the time of 0.1 ms past every millisecond.
		delay.tv_nsec = (1000 - (us - 100) % 1000) * 1000;
		nanosleep(&delay, NULL);
	}

	return NULL;
}

static void slack_init()
{
	int rc;
	pthread_attr_t attr;
	pthread_t thr;

	int64_t us = get_mono_us();
	_slack_mono_msec = us / 1000;
	_slack_mono_us = us;
	_slack_real_delta = get_real_us() - us;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 16 * 1024);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&thr, &attr, msec_thread, NULL);
	pthread_attr_destroy(&attr);

	if (rc != 0)
		abort();
}

int64_t slack_mono_msec()
{
	pthread_once(&_slack_once, slack_init);
	return _slack_mono_msec; 
}

int64_t slack_real_msec()
{
	pthread_once(&_slack_once, slack_init);
	return (_slack_mono_us + _slack_real_delta) / 1000;
}


int64_t exact_mono_msec()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int64_t exact_real_msec()
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

