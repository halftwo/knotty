#ifndef RBTREE_H_
#define RBTREE_H_

#include "xmem.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rbtree_t rbtree_t;

typedef int (*rbtree_item_compare_function)(const void *key, const void *item);

typedef void (*rbtree_item_clear_function)(void *item, void *ctx);


struct rbtree_t
{
	/* sizeof(rbnode_t) == (4 * sizeof(void*) + item_size) */
	struct rbnode_t *root;
	struct scan_info *inprog;
	size_t item_size;
	rbtree_item_compare_function compare;
	const xmem_t *xmem;
	void *xmem_ctx;
};

#define RBTREE_INIT(DATA_SIZE, COMPARE, XM, XMCTX)	{ NULL, NULL, (DATA_SIZE), (COMPARE), (XM), (XMCTX) }


static inline void rbtree_init(rbtree_t *tr, size_t item_size, rbtree_item_compare_function compare, const xmem_t *xm, void *xm_ctx)
{
	tr->root = NULL;
	tr->inprog = NULL;
	tr->item_size = item_size;
	tr->compare = compare;
	tr->xmem = xm;
	tr->xmem_ctx = xm_ctx;
}

/* The item clear function should destroy the key and value in the item. 
 * It must NOT free the memory of the item.
 */
void rbtree_finish(rbtree_t *tr, rbtree_item_clear_function clear/*NULL*/, void *ctx/*NULL*/);


/* NB: the memory of items must be freed using this function instead of free().
 * Or core dump will happen.
 * YOU HAVE BEEN WARNED.
 */
void rbtree_free_item(rbtree_t *tr, void *item);


/* Remove the item from the tree.
 * NB: The item MUST be in the node of the tree.
 * You should clear the data of the items if needed
 * and rbtree_free_item() it afterward.
 */
void rbtree_remove_item(rbtree_t *tr, void *item);


/* The following function return a pointer to the item.
 */


/* The item that equal to the key is removed from the tree.
 * You should clear the data of the items if needed
 * and rbtree_free_item() it.
 * The key itself is not removed from the tree. It's just 
 * used to find the item.
 */
void *rbtree_remove(rbtree_t *tr, const void *key);


/* A new item is allocated and inserted into the tree 
 * if there is no items equal to the key.
 * The data of the returned item should be assigned by the caller.
 * The key itself is not inserted into the tree. It's just 
 * used to find the possible equal item.
 */
void *rbtree_insert(rbtree_t *tr, const void *key);


/* Find the item that equal to the key.
 */
void *rbtree_find(rbtree_t *tr, const void *key);



void *rbtree_root(rbtree_t *tr);

void *rbtree_first(rbtree_t *tr);

void *rbtree_last(rbtree_t *tr);


void *rbtree_prev(const void *item);

void *rbtree_next(const void *item);


/* Issue a callback for all matching items.  The scancmp function must
 * return < 0 for items below the desired range, 0 for items within
 * the range, and > 0 for items beyond the range.   Any item may be
 * deleted in the callback function while the scan is in progress.
 * If callback returns a negative number, the scan will terminate and return
 * what callback returns. Otherwise, the scan will continue until all 
 * matching items are completed, and return the accumulated number of all
 * returned values of callback calls.
 */
int rbtree_scan(rbtree_t *tr, int (*scancmp)(const void *item, void *ctx)/*NULL*/,
		int (*callback)(void *item, void *ctx), void *ctx/*NULL*/);


#ifdef __cplusplus
}
#endif

#endif

