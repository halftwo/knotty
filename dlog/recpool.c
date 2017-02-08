#include "recpool.h"
#include "xslib/obpool.h"
#include <stdbool.h>
#include <pthread.h>

#define POOL_SIZE	8192
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static struct dlog_record *_pool[POOL_SIZE];
static size_t _num;


struct dlog_record *recpool_acquire()
{
	struct dlog_record *rec = NULL;

	pthread_mutex_lock(&_mutex);
	if (_num > 0)
		rec = _pool[--_num];
	pthread_mutex_unlock(&_mutex);

	if (!rec)
		rec = malloc(DLOG_RECORD_MAX_SIZE);
	return rec;
}

void recpool_release(struct dlog_record *rec)
{
	if (rec)
	{
		pthread_mutex_lock(&_mutex);
		if (_num < POOL_SIZE)
		{
			_pool[_num++] = rec;
			rec = NULL;
		}
		pthread_mutex_unlock(&_mutex);

		if (rec)
			free(rec);
	}
}

void recpool_release_all(struct dlog_record *recs[], size_t num)
{
	if (num)
	{
		pthread_mutex_lock(&_mutex);
		while (num && _num < POOL_SIZE)
		{
			struct dlog_record *rec = recs[--num];
			if (rec)
			{
				_pool[_num++] = rec;
			}
		}
		pthread_mutex_unlock(&_mutex);

		while (num--)
		{
			free(recs[num]);
		}
	}
}

