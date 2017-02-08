/* $Id: hdict.h,v 1.13 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2006-07-05
 */
#ifndef HDICT_H_
#define HDICT_H_ 1

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct hdict_t hdict_t;


/*
   The item (including Data and Key) are stored in memory as following:
        +----------+----------------------+
        |   Data   |   Key                |
        +----------+----------------------+
   The Data  must be fix-sized, the Key can be a string or fix-sized binary 
   block.  The item is stored aligned to sizeof(long) bytes in memory.  

   NB: There is no memory gap between the Data and the Key. The alignment 
   of the Key depends on the size of the Data.
 */


/*
   If key_size == 0, the 'key' is treated as a string when hdict_insert() 
   or hdict_test(). Otherwise, the 'key' is as an opaque binary block
   whose size is 'key_size'.
 */
hdict_t *hdict_create(size_t slot_num, size_t key_size, size_t data_size);


void hdict_destroy(hdict_t *hd);


/* 
   Clear the hdict_t for reuse. The slot_num, key_size, value_size 
   remain unchanged.
 */
void hdict_clear(hdict_t *hd);


size_t hdict_total(hdict_t *hd);

/*
   Return a pointer to the item with the 'key'. If no item with the
   specified 'key', NULL is returned.
 */
void *hdict_find(hdict_t *hd, const void *key);


/* Return a pointer to the inserted item associated with the 'key'.
   If the key already in the hdict_t, NULL is returned.
 */
void *hdict_insert(hdict_t *hd, const void *key, const void *data);



typedef struct
{
	hdict_t *hdict;
	void *opaque;
} hdict_iter_t;


void hdict_iter_init(hdict_t *hd, hdict_iter_t *iter);

void *hdict_iter_next(hdict_iter_t *iter, void **p_key);

void *hdict_item_key(hdict_t *hd, void *item);



void *hdict_alloc(hdict_t *hd, size_t size);

void hdict_free(hdict_t *hd, void *ptr, size_t size);


#ifdef __cplusplus
}
#endif

#endif

