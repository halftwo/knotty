/* $Id: hmap.c,v 1.2 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2008-10-15
 */
#include "hmap.h"
#include "jenkins.h"
#include "bit.h"
#include "obpool.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: hmap.c,v 1.2 2012/09/20 03:21:47 jiagui Exp $";
#endif


#define HERE_STR_KEY_SIZE	16

struct hm_item
{
	struct hm_item *next;
	unsigned int ksum;
	unsigned int ksize;
	void *data;
	char key[];
};

struct hmap_t
{
	unsigned int key_size;
	unsigned int slot_num;
	unsigned int mask;
	unsigned int max_ksize;
	struct hm_item **tab;
	size_t total;
	obpool_t item_pool;
};


hmap_t *hmap_create(size_t slot_num, size_t key_size)
{
	size_t item_size;
	hmap_t *hm = calloc(1, sizeof(hmap_t));
	if (!hm)
		goto error;

	hm->key_size = key_size;
	hm->slot_num = round_up_power_two(slot_num < INT_MAX ? slot_num : INT_MAX);
	hm->mask = hm->slot_num - 1;
	hm->tab = calloc(hm->slot_num, sizeof(struct hm_item *));
	if (!hm->tab)
		goto error;
	
	item_size = sizeof(struct hm_item) + hm->key_size;
	if (hm->key_size == 0)
		item_size += HERE_STR_KEY_SIZE;
	obpool_init(&hm->item_pool, item_size);
	return hm;
error:
	if (hm)
	{
		if (hm->tab)
			free(hm->tab);
		free(hm);
	}
	return NULL;
}


void hmap_destroy(hmap_t *hm, void (*data_destroy)(void *data))
{
	if (hm)
	{
		if (data_destroy || (hm->key_size == 0 && hm->max_ksize > HERE_STR_KEY_SIZE))
		{
			unsigned int hash;
			for (hash = 0; hash < hm->slot_num; ++hash)
			{
				struct hm_item *item;
				for (item = hm->tab[hash]; item; item = item->next)
				{
					if (data_destroy)
						data_destroy(item->data);
					if (hm->key_size == 0 && item->ksize > HERE_STR_KEY_SIZE)
						free(*(char **)item->key);
				}
			}
		}

		obpool_finish(&hm->item_pool);
		free(hm->tab);
		free(hm);
	}
}


size_t hmap_total(hmap_t *hm)
{
	return hm->total;
}


struct hm_item **_find_pp(hmap_t *hm, const void *key)
{
	struct hm_item *item, **item_pp = NULL;
	int ksize = hm->key_size ? hm->key_size : strlen(key) + 1;
	unsigned int ksum = jenkins_hash(key, ksize, 0);
	unsigned int hash = (ksum & hm->mask);

	item_pp = &hm->tab[hash];
	if (hm->key_size || ksize <= HERE_STR_KEY_SIZE)
	{
		for (item = *item_pp; item; item_pp = &item->next, item = *item_pp)
		{
			if (item->ksum == ksum && item->ksize == ksize && memcmp(item->key, key, ksize) == 0)
				break;
		}
	}
	else
	{
		for (item = *item_pp; item; item_pp = &item->next, item = *item_pp)
		{
			if (item->ksum == ksum && item->ksize == ksize && memcmp(*(char **)&item->key, key, ksize) == 0)
				break;
		}
	}

	return item ? item_pp : NULL;
}

bool hmap_insert(hmap_t *hm, const void *key, const void *data)
{
	struct hm_item *item, **item_pp = NULL;
	int ksize = hm->key_size ? hm->key_size : strlen(key) + 1;
	unsigned int ksum = jenkins_hash(key, ksize, 0);
	unsigned int hash = (ksum & hm->mask);

	item_pp = &hm->tab[hash];
	if (hm->key_size || ksize <= HERE_STR_KEY_SIZE)
	{
		for (item = *item_pp; item; item_pp = &item->next, item = *item_pp)
		{
			if (item->ksum == ksum && item->ksize == ksize && memcmp(item->key, key, ksize) == 0)
				break;
		}
	}
	else
	{
		for (item = *item_pp; item; item_pp = &item->next, item = *item_pp)
		{
			if (item->ksum == ksum && item->ksize == ksize && memcmp(*(char **)&item->key, key, ksize) == 0)
				break;
		}
	}

	if (item)
		return false;

	item = obpool_acquire(&hm->item_pool);
	if (!item)
		return false;

	item->ksum = ksum;
	item->ksize = ksize;
	item->data = (void *)data;
	if (hm->key_size || item->ksize <= HERE_STR_KEY_SIZE)
		memcpy(item->key, key, item->ksize);
	else
	{
		char *the_key = malloc(item->ksize);
		memcpy(the_key, key, item->ksize);
		*(char **)item->key = the_key;
	}
	item->next = hm->tab[hash];
	hm->tab[hash] = item;
	hm->total++;
	if (hm->max_ksize < ksize)
		hm->max_ksize = ksize;
	return true;
}

void *hmap_find(hmap_t *hm, const void *key)
{
	struct hm_item **item_pp = _find_pp(hm, key);
	return item_pp ? (*item_pp)->data : NULL;
}

void *hmap_remove(hmap_t *hm, const void *key)
{
	void *data = NULL;
	struct hm_item **item_pp = _find_pp(hm, key);

	if (item_pp)
	{
		struct hm_item *item = *item_pp;
		*item_pp = item->next;
		data = item->data;
		if (hm->key_size == 0 && item->ksize > HERE_STR_KEY_SIZE)
		{
			char *the_key = *(char **)&item->key;
			free(the_key);
		}
	}

	return data;
}


void hmap_iter_init(hmap_t *hm, hmap_iter_t *iter)
{
	unsigned int hash;
	iter->hmap = hm;
	iter->opaque = NULL;

	for (hash = 0; hash < hm->slot_num; ++hash)
	{
		if (hm->tab[hash])
		{
			iter->opaque = hm->tab[hash];
			break;
		}
	}
}

void *hmap_iter_next(hmap_iter_t *iter, void **p_key)
{
	hmap_t *hm = iter->hmap;
	struct hm_item *item = (struct hm_item *)iter->opaque;

	if (!item)
	{
		if (p_key)
			*p_key = NULL;
		return NULL;
	}

	iter->opaque = item->next;
	if (!iter->opaque)
	{
		unsigned int hash = (item->ksum & hm->mask);

		for (++hash; hash < hm->slot_num; ++hash)
		{
			if (hm->tab[hash])
			{
				iter->opaque = hm->tab[hash];
				break;
			}
		}
	}

	if (p_key)
	{
		if (hm->key_size || item->ksize <= HERE_STR_KEY_SIZE)
			*p_key = item->key;
		else
			*p_key = *(char **)&item->key;
	}
	return item->data;
}

#ifdef TEST_HMAP

#include <stdio.h>
#include <assert.h>

int main()
{
	hmap_t *hm = hmap_create(4, 0);
	int i;
	char label[256];
	char *key;
	void *value;
	hmap_iter_t iter;

	for (i = 1; i <= 10; ++i)
	{
		sprintf(label, "label_%d", i);
		value = hmap_find(hm, label);
		assert(value == 0);
		hmap_insert(hm, label, (void *)i);
	}
	hmap_insert(hm, label, (void *)i);

	for (i = 1; i <= 10; ++i)
	{
		sprintf(label, "label_%d", i);
		value = hmap_find(hm, label);
		assert(value != 0);
	}
	
	hmap_iter_init(hm, &iter);
	while ((value = hmap_iter_next(&iter, (void **)&key)) != 0)
	{
		printf("%s = %p\n", key, value);
	}
	return 0;
}

#endif
