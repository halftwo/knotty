/* $Id: hmap.h,v 1.2 2012/09/20 03:21:47 jiagui Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2008-10-15
 */
#ifndef HMAP_H_
#define HMAP_H_ 1

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct hmap_t hmap_t;


/*
   If key_size == 0, the 'key' is treated as a string when hmap_insert() 
   or hmap_test(). Otherwise, the 'key' is as an opaque binary block
   whose size is 'key_size'.
 */
hmap_t *hmap_create(size_t slot_num, size_t key_size);

void hmap_destroy(hmap_t *hm, void (*data_destroy)(void *data));

size_t hmap_total(hmap_t *hm);



void *hmap_find(hmap_t *hm, const void *key);

bool hmap_insert(hmap_t *hm, const void *key, const void *data); 

void *hmap_remove(hmap_t *hm, const void *key);



typedef struct
{ 
	hmap_t *hmap;
	void *opaque;
} hmap_iter_t;


void hmap_iter_init(hmap_t *hm, hmap_iter_t *iter);

void *hmap_iter_next(hmap_iter_t *iter, void **p_key);



#ifdef __cplusplus
}
#endif

#endif


