#include "cirqueue.h"
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>


struct cirqueue_t
{
	pthread_mutex_t mutex;
	pthread_cond_t cond_full;
	pthread_cond_t cond_empty;
	size_t wait_full;
	size_t wait_empty;

	size_t start;
	size_t end;
	size_t used;
	size_t size;
	void *items[];
};


size_t cirqueue_used(cirqueue_t *q)
{
	return q->used;
}


size_t cirqueue_size(cirqueue_t *q)
{
	return q->size;
}


cirqueue_t *cirqueue_create(size_t size)
{
	cirqueue_t *q = NULL;
	if (size < 1)
		goto error;

	q = (cirqueue_t *)calloc(1, sizeof(q[0]) + size * sizeof(void *));
	if (q == NULL)
		goto error;

	q->size = size;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond_full, NULL);
	pthread_cond_init(&q->cond_empty, NULL);
	return q;

error:
	if (q)
		free(q);
	return NULL;
}


void cirqueue_destroy(cirqueue_t *q)
{
	if (q)
	{
		pthread_mutex_destroy(&q->mutex);
		pthread_cond_destroy(&q->cond_full);
		pthread_cond_destroy(&q->cond_empty);
		free(q);
	}
}


bool cirqueue_put(cirqueue_t *q, void *element, bool block)
{
	pthread_mutex_lock(&q->mutex);

	if (q->used == q->size)
	{
		if (block)
		{
			q->wait_full++;
			do
			{
				pthread_cond_wait(&q->cond_full, &q->mutex);
			} while (q->used == q->size);
			q->wait_full--;
		}
		else
		{
			element = NULL;
		}
	}

	if (element)
	{
		q->items[q->end++] = element;
		if (q->end == q->size)
			q->end = 0;
		q->used++;

		if (q->wait_empty)
			pthread_cond_signal(&q->cond_empty);
	}

	pthread_mutex_unlock(&q->mutex);
	return element ? true : false;
}


void *cirqueue_get(cirqueue_t *q, bool block)
{
	void *element = NULL;

	pthread_mutex_lock(&q->mutex);

	if (q->used == 0)
	{
		if (block)
		{
			q->wait_empty++;
			do
			{
				pthread_cond_wait(&q->cond_empty, &q->mutex);
			} while (q->used == 0);
			q->wait_empty--;
		}
		else
			goto done;
	}

	element = q->items[q->start++];
	assert(element);
	if (q->start == q->size)
		q->start = 0;
	q->used--;

	if (q->wait_full)
		pthread_cond_signal(&q->cond_full);

done:
	pthread_mutex_unlock(&q->mutex);
	return element;
}

size_t cirqueue_putv(cirqueue_t *q, void *elements[], size_t num, bool block)
{
	size_t n = 0;
	size_t real = 0;

	pthread_mutex_lock(&q->mutex);

	if (q->used == q->size)
	{
		if (block)
		{
			q->wait_full++;
			do
			{
				pthread_cond_wait(&q->cond_full, &q->mutex);
			} while (q->used == q->size);
			q->wait_full--;
		}
		else
			goto done;
	}

	for (n = 0; n < num && q->used < q->size; ++n)
	{
		void *e = elements[n];
		if (e)
		{
			q->items[q->end++] = e;
			if (q->end == q->size)
				q->end = 0;
			q->used++;
			++real;
		}
	}

	if (q->wait_empty && real)
	{
		if (real > 1)
			pthread_cond_broadcast(&q->cond_empty);
		else
			pthread_cond_signal(&q->cond_empty);
	}

done:
	pthread_mutex_unlock(&q->mutex);
	return n;
}

size_t cirqueue_getv(cirqueue_t *q, void *elements[], size_t num, bool block)
{
	size_t n = 0;

	pthread_mutex_lock(&q->mutex);

	if (q->used == 0)
	{
		if (block)
		{
			q->wait_empty++;
			do
			{
				pthread_cond_wait(&q->cond_empty, &q->mutex);
			} while (q->used == 0);
			q->wait_empty--;
		}
		else
			goto done;
	}

	for (n = 0; n < num && q->used; ++n)
	{
		elements[n] = q->items[q->start++];
		if (q->start == q->size)
			q->start = 0;
		q->used--;
	}

	if (q->wait_full)
	{
		if (n > 1)
			pthread_cond_broadcast(&q->cond_full);
		else
			pthread_cond_signal(&q->cond_full);
	}

done:
	pthread_mutex_unlock(&q->mutex);
	return n;
}

size_t cirqueue_timed_putv(cirqueue_t *q, void *elements[], size_t num, const struct timespec *abstime)
{
	size_t n = 0;
	size_t real = 0;

	pthread_mutex_lock(&q->mutex);

	if (q->used == q->size)
	{
		q->wait_full++;
		do
		{
			int rc = pthread_cond_wait(&q->cond_full, &q->mutex);
			if (rc == ETIMEDOUT)
				break;
		} while (q->used == q->size);
		q->wait_full--;
	}

	for (n = 0; n < num && q->used < q->size; ++n)
	{
		void *e = elements[n];
		if (e)
		{
			q->items[q->end++] = e;
			if (q->end == q->size)
				q->end = 0;
			q->used++;
			++real;
		}
	}

	if (q->wait_empty && real)
	{
		if (real > 1)
			pthread_cond_broadcast(&q->cond_empty);
		else
			pthread_cond_signal(&q->cond_empty);
	}

	pthread_mutex_unlock(&q->mutex);
	return n;
}

size_t cirqueue_timed_getv(cirqueue_t *q, void *elements[], size_t num, const struct timespec *abstime)
{
	size_t n = 0;

	pthread_mutex_lock(&q->mutex);

	if (q->used == 0)
	{
		q->wait_empty++;
		do
		{
			int rc = pthread_cond_timedwait(&q->cond_empty, &q->mutex, abstime);
			if (rc == ETIMEDOUT)
				break;
		} while (q->used == 0);
		q->wait_empty--;
	}

	for (n = 0; n < num && q->used; ++n)
	{
		elements[n] = q->items[q->start++];
		if (q->start == q->size)
			q->start = 0;
		q->used--;
	}

	if (q->wait_full && n)
	{
		if (n > 1)
			pthread_cond_broadcast(&q->cond_full);
		else
			pthread_cond_signal(&q->cond_full);
	}

	pthread_mutex_unlock(&q->mutex);
	return n;
}

