/* $Id: hdict.c,v 1.25 2015/05/18 07:12:50 gremlin Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-07-05
 */
#include "hdict.h"
#include "jenkins.h"
#include "ostk.h"
#include "bit.h"
#include "obpool.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: hdict.c,v 1.25 2015/05/18 07:12:50 gremlin Exp $";
#endif

struct hd_item_t
{
	struct hd_item_t *next;
	unsigned long ksum;
	char buf[];
};

#define MIN_SHIFT	3
#define MAX_SHIFT	12

struct hdict_t
{
	ostk_t *ostk;
	unsigned int key_size;
	unsigned int data_size;
	unsigned int mask;
	unsigned int slot_num;
	struct hd_item_t **tab;
	size_t total;
	obpool_t mps[MAX_SHIFT + 1];
};


hdict_t *hdict_create(size_t slot_num, size_t key_size, size_t data_size)
{
	int i;
	ostk_t *ostk = ostk_create(0);
	hdict_t *hd = (hdict_t *)ostk_hold(ostk, sizeof(hdict_t));
	hd->ostk = ostk;
	hd->slot_num = round_up_power_two(slot_num < INT_MAX ? slot_num : INT_MAX);
	hd->mask = hd->slot_num - 1;
	hd->key_size = key_size;
	hd->data_size = data_size;
	hd->tab = (struct hd_item_t **)ostk_calloc(hd->ostk, hd->slot_num * sizeof(struct hd_item_t *));
	for (i = MIN_SHIFT; i <= MAX_SHIFT; ++i)
		obpool_init(&hd->mps[i], 1 << i);
	return hd;
}

void hdict_clear(hdict_t *hd)
{
	int i;
	for (i = MIN_SHIFT; i <= MAX_SHIFT; ++i)
		obpool_finish(&hd->mps[i]);
	
	ostk_clear(hd->ostk);
	hd->tab = (struct hd_item_t **)ostk_calloc(hd->ostk, hd->slot_num * sizeof(struct hd_item_t *));

	for (i = MIN_SHIFT; i <= MAX_SHIFT; ++i)
		obpool_init(&hd->mps[i], 1 << i);

	hd->total = 0;
}

void hdict_destroy(hdict_t *hd)
{
	int i;
	for (i = MIN_SHIFT; i <= MAX_SHIFT; ++i)
		obpool_finish(&hd->mps[i]);
	
	ostk_destroy(hd->ostk);
}

size_t hdict_total(hdict_t *hd)
{
	return hd->total;
}

void *hdict_insert(hdict_t *hd, const void *key, const void *data)
{
	struct hd_item_t *item;
	int len = hd->key_size ? hd->key_size : strlen((char *)key) + 1;
	unsigned int ksum = jenkins_hash(key, len, 0);
	unsigned int hash = (ksum & hd->mask);

	for (item = hd->tab[hash]; item; item = item->next)
	{
		if (item->ksum == ksum && memcmp(item->buf + hd->data_size, key, len) == 0)
			break;
	}

	if (item)
		return NULL;

	int size = sizeof(struct hd_item_t) + hd->data_size + len;
	item = (struct hd_item_t *)ostk_alloc(hd->ostk, size);
	if (!item)
		return NULL;
	item->ksum = ksum;
	if (data)
		memcpy(item->buf, data, hd->data_size);
	else
		memset(item->buf, 0, hd->data_size);
	memcpy(item->buf + hd->data_size, key, len);
	item->next = hd->tab[hash];
	hd->tab[hash] = item;
	hd->total++;
	return item->buf;
}

void *hdict_find(hdict_t *hd, const void *key)
{
	struct hd_item_t *item;
	int len = hd->key_size ? hd->key_size : strlen((char *)key) + 1;
	unsigned int ksum = jenkins_hash(key, len, 0);
	unsigned int hash = (ksum & hd->mask);

	for (item = hd->tab[hash]; item; item = item->next)
	{
		if (item->ksum == ksum && memcmp(item->buf + hd->data_size, key, len) == 0)
			break;
	}

	if (!item)
		return NULL;

	return item->buf;
}

void hdict_iter_init(hdict_t *hd, hdict_iter_t *iter)
{
	unsigned int hash;
	iter->hdict = hd;
	iter->opaque = NULL;

	for (hash = 0; hash < hd->slot_num; ++hash)
	{
		if (hd->tab[hash])
		{
			iter->opaque = hd->tab[hash];
			break;
		}
	}
}

void *hdict_iter_next(hdict_iter_t *iter, void **p_key)
{
	hdict_t *hd = iter->hdict;
	struct hd_item_t *item = (struct hd_item_t *)iter->opaque;

	if (!item)
	{
		if (p_key)
			*p_key = NULL;
		return NULL;
	}

	iter->opaque = item->next;
	if (!iter->opaque)
	{
		unsigned int hash = (item->ksum & hd->mask);

		for (++hash; hash < hd->slot_num; ++hash)
		{
			if (hd->tab[hash])
			{
				iter->opaque = hd->tab[hash];
				break;
			}
		}
	}

	if (p_key)
		*p_key = item->buf + hd->data_size;
	return item->buf;
}

void *hdict_item_key(hdict_t *hd, void *item)
{
	/* NB: the /item/ is in fact the item->buf. */
	return item ? ((char *)item + hd->data_size) : NULL;
}

static int find_shift(unsigned int x)
{
	int r = 1;

	if (x == 0)
		return 0;
	--x;
	while (x >>= 1)
		r++;

	return r;
}

/* Can only allocate memory that size <= 4096. */
void *hdict_alloc(hdict_t *hd, size_t size)
{
	if (size <= (1 << 12))
	{
		int shift = find_shift(size);
		if (shift < 3)
			shift = 3;

		return obpool_acquire(&hd->mps[shift]);
	}

	return NULL;
}

/* Must supply the size of the allocated memory. */
void hdict_free(hdict_t *hd, void *ptr, size_t size)
{
	if (ptr)
	{
		if (size <= (1 << 12))
		{
			int shift = find_shift(size);
			if (shift < 3)
				shift = 3;

			obpool_release(&hd->mps[shift], ptr);
		}
	}
}

