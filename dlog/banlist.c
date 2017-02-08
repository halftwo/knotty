#include "banlist.h"
#include "xslib/jenkins.h"
#include "xslib/ostk.h"
#include "xslib/bit.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct banlist_t
{
};


struct b_item
{
	struct b_item *next;
	uint32_t ksum;
	unsigned long category;
	bool catchall;
	char str[];
};

typedef struct
{
	ostk_t *ostk;
	uint32_t mask;
	unsigned int slot_num;
	struct b_item **tab;
	unsigned int total;
} bdict_t;

bdict_t *bdict_create(unsigned int slot_num)
{
	bdict_t *bd = NULL;
	ostk_t *ostk = ostk_create(0);
	bd = OSTK_CALLOC_ONE(ostk, bdict_t);
	bd->ostk = ostk;
	bd->slot_num = round_up_power_two(slot_num);
	bd->mask = bd->slot_num - 1;
	bd->tab = OSTK_CALLOC(bd->ostk, struct b_item *, bd->slot_num);
	return bd;
}

void bdict_destroy(bdict_t *bd)
{
	ostk_destroy(bd->ostk);
}


struct b_item *bdict_insert(bdict_t *bd, unsigned long category, const char *key)
{
	struct b_item *b_item;
	int len = strlen(key) + 1;
	uint32_t sum = jenkins_hash(key, len, jenkins_hash(&category, sizeof(category), 0));
	uint32_t hash = (sum & bd->mask);

	for (b_item = bd->tab[hash]; b_item; b_item = b_item->next)
	{
		if (b_item->ksum == sum && b_item->category == category && memcmp(b_item->str, key, len) == 0)
			break;
	}

	if (!b_item)
	{
		int size = offsetof(struct b_item, str) + len;
		b_item = ostk_alloc(bd->ostk, size);
		if (!b_item)
			return NULL;
		b_item->ksum = sum;
		b_item->category = category;
		b_item->catchall = false;
		memcpy(b_item->str, key, len);
		b_item->next = bd->tab[hash];
		bd->tab[hash] = b_item;
		bd->total++;
	}

	return b_item;
}


struct b_item *bdict_test(bdict_t *bd, unsigned long category, const char *key)
{
	struct b_item *b_item;
	int len = strlen(key) + 1;
	uint32_t sum = jenkins_hash(key, len, jenkins_hash(&category, sizeof(category), 0));
	uint32_t hash = (sum & bd->mask);

	for (b_item = bd->tab[hash]; b_item; b_item = b_item->next)
	{
		if (b_item->ksum == sum && b_item->category == category && memcmp(b_item->str, key, len) == 0)
			break;
	}

	return b_item;
}


banlist_t *banlist_load(const char *filename)
{
	return NULL;
}

void banlist_close(banlist_t *b)
{
}


/*
 
 ip identity tag
 123.213.132.231 * *

 */

bool banlist_check(banlist_t *b, const char *ip, struct dlog_record *rec)
{
	return false;
}

