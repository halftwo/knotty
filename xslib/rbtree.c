#include "rbtree.h"
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: rbtree.c,v 1.3 2015/05/21 03:39:02 gremlin Exp $";
#endif


enum rbcolor
{
	INVALID_COLOR = -1,
	BLACK_COLOR = 0,
	RED_COLOR = 1,
};

typedef struct rbnode_t rbnode_t;
struct rbnode_t
{
	enum rbcolor color;
	rbnode_t *left;
	rbnode_t *right;
	rbnode_t *parent;
	unsigned char item[];
};

struct scan_info
{
	struct scan_info *link;
	rbnode_t *node;
};

static inline rbnode_t *_rb_min(rbtree_t *tr)
{
	rbnode_t *node = tr->root;
	rbnode_t *parent = NULL;
	while (node)
	{
		parent = node;
		node = node->left;
	}
	return parent;
}

static inline rbnode_t *_rb_max(rbtree_t *tr)
{
	rbnode_t *node = tr->root;
	rbnode_t *parent = NULL;
	while (node)
	{
		parent = node;
		node = node->right;
	}
	return parent;
}

static inline rbnode_t *_rb_prev(rbnode_t *node)
{
	if (node->left)
	{
		node = node->left;
		while (node->right)
			node = node->right;
	}
	else
	{
		if (node->parent && node == node->parent->right)
		{
			node = node->parent;
		}
		else
		{
			while (node->parent && node == node->parent->left)
				node = node->parent;
			node = node->parent;
		}
	}
	return node;
}

static inline rbnode_t *_rb_next(rbnode_t *node)
{
	if (node->right)
	{
		node = node->right;
		while (node->left)
			node = node->left;
	}
	else
	{
		if (node->parent && node == node->parent->left)
		{
			node = node->parent;
		}
		else
		{
			while (node->parent && node == node->parent->right)
				node = node->parent;
			node = node->parent;
		}
	}
	return node;
}


#define RB_ROTATE_LEFT(tr, node, tmp)		do {		\
	(tmp) = node->right;					\
	if (((node)->right = (tmp)->left) != NULL) {		\
		(tmp)->left->parent = (node);			\
	}							\
	if (((tmp)->parent = (node)->parent) != NULL) {		\
		if ((node) == (node)->parent->left)		\
			(node)->parent->left = (tmp);		\
		else						\
			(node)->parent->right = (tmp);		\
	} else {						\
		(tr)->root = (tmp);				\
	}							\
	(tmp)->left = (node);					\
	(node)->parent = (tmp);					\
} while (0)


#define RB_ROTATE_RIGHT(tr, node, tmp)		do {		\
	(tmp) = (node)->left;					\
	if (((node)->left = (tmp)->right) != NULL) {		\
		(tmp)->right->parent = (node);			\
	}							\
	if (((tmp)->parent = (node)->parent) != NULL) {		\
		if ((node) == (node)->parent->left)		\
			(node)->parent->left = (tmp);		\
		else						\
			(node)->parent->right = (tmp);		\
	} else {						\
		(tr)->root = (tmp);				\
	}							\
	(tmp)->right = (node);					\
	(node)->parent = (tmp);					\
} while (0)


#define RB_SET_BLACKRED(black, red) 		do {		\
	(black)->color = BLACK_COLOR;				\
	(red)->color = RED_COLOR;				\
} while (0)


static void _rb_insert_color(rbtree_t *tr, rbnode_t *node)
{
	rbnode_t *parent, *gp, *tmp;
	while ((parent = node->parent) != NULL && parent->color == RED_COLOR)
	{
		gp = parent->parent;
		if (parent == gp->left)
		{
			tmp = gp->right;
			if (tmp && tmp->color == RED_COLOR)
			{
				tmp->color = BLACK_COLOR;
				RB_SET_BLACKRED(parent, gp);
				node = gp;
				continue;
			}
			if (parent->right == node)
			{
				RB_ROTATE_LEFT(tr, parent, tmp);
				tmp = parent;
				parent = node;
				node = tmp;
			}
			RB_SET_BLACKRED(parent, gp);
			RB_ROTATE_RIGHT(tr, gp, tmp);
		}
		else
		{
			tmp = gp->left;
			if (tmp && tmp->color == RED_COLOR)
			{
				tmp->color = BLACK_COLOR;
				RB_SET_BLACKRED(parent, gp);
				node = gp;
				continue;
			}
			if (parent->left == node)
			{
				RB_ROTATE_RIGHT(tr, parent, tmp);
				tmp = parent;
				parent = node;
				node = tmp;
			}
			RB_SET_BLACKRED(parent, gp);
			RB_ROTATE_LEFT(tr, gp, tmp);
		}
	}
	tr->root->color = BLACK_COLOR;
}

static void _rb_remove_color(rbtree_t *tr, rbnode_t *parent, rbnode_t *node)
{
	rbnode_t *tmp;
	while ((node == NULL || node->color == BLACK_COLOR) && node != tr->root)
	{
		if (parent->left == node)
		{
			tmp = parent->right;
			if (tmp->color == RED_COLOR)
			{
				RB_SET_BLACKRED(tmp, parent);
				RB_ROTATE_LEFT(tr, parent, tmp);
				tmp = parent->right;
			}

			if ((tmp->left == NULL || tmp->left->color == BLACK_COLOR)
				&& (tmp->right == NULL || tmp->right->color == BLACK_COLOR))
			{
				tmp->color = RED_COLOR;
				node = parent;
				parent = node->parent;
			}
			else
			{
				if (tmp->right == NULL || tmp->right->color == BLACK_COLOR)
				{
					rbnode_t *oleft = tmp->left;
					if (oleft)
						oleft->color = BLACK_COLOR;
					tmp->color = RED_COLOR;
					RB_ROTATE_RIGHT(tr, tmp, oleft);
					tmp = parent->right;
				}
				tmp->color = parent->color;
				parent->color = BLACK_COLOR;
				if (tmp->right)
					tmp->right->color = BLACK_COLOR;
				RB_ROTATE_LEFT(tr, parent, tmp);
				node = tr->root;
				break;
			}
		}
		else 
		{
			tmp = parent->left;
			if (tmp->color == RED_COLOR)
			{
				RB_SET_BLACKRED(tmp, parent);
				RB_ROTATE_RIGHT(tr, parent, tmp);
				tmp = parent->left;
			}

			if ((tmp->left == NULL || tmp->left->color == BLACK_COLOR)
				&& (tmp->right == NULL || tmp->right->color == BLACK_COLOR))
			{
				tmp->color = RED_COLOR;
				node = parent;
				parent = node->parent;
			}
			else
			{
				if (tmp->left == NULL || tmp->left->color == BLACK_COLOR)
				{
					rbnode_t *oright = tmp->right;
					if (oright)
						oright->color = BLACK_COLOR;
					tmp->color = RED_COLOR;
					RB_ROTATE_LEFT(tr, tmp, oright);
					tmp = parent->left;
				}
				tmp->color = parent->color;
				parent->color = BLACK_COLOR;
				if (tmp->left)
					tmp->left->color = BLACK_COLOR;
				RB_ROTATE_RIGHT(tr, parent, tmp);
				node = tr->root;
				break;
			}
		}
	}

	if (node)
	{
		node->color = BLACK_COLOR;
	}
}

static rbnode_t *_rb_remove(rbtree_t *tr, rbnode_t *node)
{
	rbnode_t *child, *parent, *old;
	struct scan_info *inprog;
	int color;

	for (inprog = tr->inprog; inprog; inprog = inprog->link)
	{
		if (inprog->node == node)
			inprog->node = _rb_next(node);
	}

	old = node;
	if (node->left == NULL)
	{
		child = node->right;
	}
	else if (node->right == NULL)
	{
		child = node->left;
	}
	else
	{
		rbnode_t *left;
		node = node->right;
		while ((left = node->left) != NULL)
			node = left;

		child = node->right;
		parent = node->parent;
		color = node->color;
		if (child)
			child->parent = parent;

		if (parent)
		{
			if (parent->left == node)
				parent->left = child;
			else
				parent->right = child;
		}
		else
		{
			tr->root = child;
		}

		if (node->parent == old)
			parent = node;

		*node = *old;
		if (old->parent)
		{
			if (old->parent->left == old)
				old->parent->left = node;
			else
				old->parent->right = node;
		}
		else
		{
			tr->root = node;
		}

		old->left->parent = node;
		if (old->right)
			old->right->parent = node;

		goto color;
	}

	parent = node->parent;
	color = node->color;
	if (child)
		child->parent = parent;

	if (parent)
	{
		if (parent->left == node)
			parent->left = child;
		else
			parent->right = child;
	}
	else
	{
		tr->root = child;
	}
color:
	if (color == BLACK_COLOR)
		_rb_remove_color(tr, parent, child);

	return old;
}

static inline void _rb_node_unset(rbnode_t *node)
{
	node->color = INVALID_COLOR;
	node->parent = NULL;
	node->left = NULL;
	node->right = NULL;
}

void rbtree_finish(rbtree_t *tr, rbtree_item_clear_function clear, void *ctx)
{
	xmem_free_function xm_free = tr->xmem ? tr->xmem->free : stdc_xmem.free;	/* May be NULL */

	assert(tr->inprog == NULL);
	while (1)
	{
		rbnode_t *node = tr->root;
		if (!node)
			break;

		_rb_remove(tr, node);
		if (clear)
			clear(node->item, ctx);
		if (xm_free)
			xm_free(tr->xmem_ctx, node);
	}
	tr->root = NULL;
}

void rbtree_free_item(rbtree_t *tr, void *item)
{
	rbnode_t *node = ((rbnode_t*)item)-1;
	if (tr->xmem)
	{
		if (tr->xmem->free)
			tr->xmem->free(tr->xmem_ctx, node);
	}
	else
	{
		free(node);
	}
}

void rbtree_remove_item(rbtree_t *tr, void *item)
{
	rbnode_t *node = ((rbnode_t *)item)-1;
	assert(node->color != INVALID_COLOR);
	_rb_remove(tr, node);
	_rb_node_unset(node);
}

static inline rbnode_t *_rb_allocate_node(rbtree_t *tr)
{
	rbnode_t *node;
	if (tr->xmem)
	{
		node = tr->xmem->alloc(tr->xmem_ctx, sizeof(rbnode_t) + tr->item_size);
		memset(node, 0, sizeof(rbnode_t) + tr->item_size);
	}
	else
	{
		node = (rbnode_t*)calloc(1, sizeof(rbnode_t) + tr->item_size);
	}
	return node;
}

void *rbtree_insert(rbtree_t *tr, const void *key)
{
	rbnode_t *node, *tmp, *parent = NULL;
	int comp = 0;

	tmp = tr->root;
	while (tmp)
	{
		parent = tmp;
		comp = tr->compare(key, parent->item);
		if (comp < 0)
			tmp = tmp->left;
		else if (comp > 0)
			tmp = tmp->right;
		else
			return NULL;
	}

	node = _rb_allocate_node(tr);
	if (!node)
		return NULL;

	node->parent = parent;
	node->color = RED_COLOR;

	if (parent != NULL)
	{
		if (comp < 0)
			parent->left = node;
		else
			parent->right = node;
	}
	else
	{
		tr->root = node;
	}

	_rb_insert_color(tr, node);
	return node->item;
}

void *rbtree_find(rbtree_t *tr, const void *key)
{
	rbnode_t *node = tr->root;
	while (node)
	{
		int comp = tr->compare(key, node->item);
		if (comp < 0)
			node = node->left;
		else if (comp > 0)
			node = node->right;
		else
			return node->item;
	}
	return NULL;
}

void *rbtree_remove(rbtree_t *tr, const void *key)
{
	void *item = rbtree_find(tr, key);
	if (item)
	{
		rbnode_t *node = ((rbnode_t *)item) - 1;
		_rb_remove(tr, node);
		_rb_node_unset(node);
		return node->item;
	}
	return NULL;
}

void *rbtree_root(rbtree_t *tr)
{
	rbnode_t *node = tr->root;
	return node ? node->item : NULL;
}

void *rbtree_first(rbtree_t *tr)
{
	rbnode_t *node = _rb_min(tr);
	return node ? node->item : NULL;
}

void *rbtree_last(rbtree_t *tr)
{
	rbnode_t *node = _rb_max(tr);
	return node ? node->item : NULL;
}

void *rbtree_next(const void *item)
{
	rbnode_t *tmp = _rb_next(((rbnode_t*)item)-1);
	return tmp ? tmp->item : NULL;
}

void *rbtree_prev(const void *item)
{
	rbnode_t *tmp = _rb_prev(((rbnode_t*)item)-1);
	return tmp ? tmp->item : NULL;
}


int rbtree_scan(rbtree_t *tr, int (*match)(const void *item, void *ctx) /*NULL*/,
		int (*callback)(void *item, void *ctx), void *ctx)
{
	struct scan_info info;
	struct scan_info **infopp;
	rbnode_t *node, *tmp;
	int count;
	int comp;

	/* Locate the first node. */
	tmp = tr->root;
	node = NULL;
	while (tmp)
	{
		comp = match ? match(tmp->item, ctx) : 0;
		if (comp < 0)
		{	
			tmp = tmp->right;
		}
		else if (comp > 0)
		{
			tmp = tmp->left;
		}
		else 
		{
			node = tmp;
			if (tmp->left == NULL)
				break;
			tmp = tmp->left;
		}
	}

	count = 0;
	if (node)
	{
		info.node = _rb_next(node);
		info.link = tr->inprog;
		tr->inprog = &info;
		while ((comp = callback(node->item, ctx)) >= 0)
		{
			count += comp;
			node = info.node;
			if (node == NULL || (match && match(node->item, ctx) != 0))
				break;
			info.node = _rb_next(node);
		}

		if (comp < 0)
		{
			/* error or termination */
			count = comp;
		}

		infopp = &tr->inprog;
		while (*infopp != &info)
			infopp = &(*infopp)->link;
		*infopp = info.link;
	}

	return count;
}


#ifdef TEST_RBTREE

#include <stdio.h>

typedef struct item_t item_t;
struct item_t
{
	int key;
	int value;
};

static int comp(const void *a, const void *b)
{
	item_t *x = (item_t *)a;
	item_t *y = (item_t *)b;
	return x->key < y->key ? -1 : x->key > y->key ? 1 : 0;
}

static int scan_callback(void *item, void *ctx)
{
	item_t *d = (item_t *)item;
	rbtree_t *tree = (rbtree_t *)ctx;
	printf("item %p %d = %d\n", item, d->key, d->value);
	rbtree_remove_item(tree, item);
	return 1;
}

int main(int argc, char **argv)
{
	int i;
	rbtree_t tree = RBTREE_INIT(sizeof(item_t), comp, NULL, NULL);
	for (i = 0; i < 1024; ++i)
	{
		item_t hint;
		hint.key = i;
		item_t *item = rbtree_insert(&tree, &hint);
		item->key = i;
		item->value = 10000 + i;
	}

	i = rbtree_scan(&tree, NULL, scan_callback, &tree);
	printf("scan %d\n", i);

	printf("tree: root=%p inprog=%p\n", tree.root, tree.inprog);
	rbtree_finish(&tree, NULL, NULL);
	return 0;
}


#endif
