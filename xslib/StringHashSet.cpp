#include "StringHashSet.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: StringHashSet.cpp,v 1.2 2010/08/06 08:36:02 jiagui Exp $";
#endif

StringHashSet::StringHashSet(size_t size)
{
	size = round_up_power_two(size > INT_MAX ? INT_MAX: size < 16 ? 16 : size);
	_tab = (Node **)calloc(size, sizeof(*_tab));
	_mask = size - 1;
	_total = 0;
}

StringHashSet::~StringHashSet()
{
	for (uint32_t slot = 0; slot <= _mask; ++slot)
	{
		Node *node, *next;
		for (node = _tab[slot]; node; node = next)
		{
			next = node->hash_next;
			node->Node::~Node();
			free(node);
		}
	}
	free(_tab);
}

StringHashSet::NodeType* StringHashSet::insert(const char *key, size_t len)
{
	Node *node;
	uint32_t ksum = jenkins_hash(key, len, 0);
	uint32_t slot = (ksum & _mask);

	for (node = _tab[slot]; node; node = node->hash_next)
	{
		if (node->ksum == ksum && node->klen == len && memcmp(node->key, key, len) == 0)
			return NULL;
	}

	void *p = malloc(sizeof(Node) + len + 1);
	node = new(p) Node(key, len);
	node->hash_next = _tab[slot];
	_tab[slot] = node;
	_total++;
	return node;
}

bool StringHashSet::remove(const char *key, size_t len)
{
	Node *node, *prev = NULL;
	uint32_t ksum = jenkins_hash(key, len, 0);
	uint32_t slot = (ksum & _mask);

	for (node = _tab[slot]; node; prev = node, node = node->hash_next)
	{
		if (node->ksum == ksum && node->klen == len && memcmp(node->key, key, len) == 0)
		{
			if (prev)
				prev->hash_next = node->hash_next;
			else
				_tab[slot] = node->hash_next;
			_total--;
			node->Node::~Node();
			free(node);
			return true;
		}
	}

	return false;
}

void StringHashSet::remove(NodeType* theNode)
{
	Node *node, *prev = NULL;
	uint32_t slot = (theNode->ksum & _mask);

	for (node = _tab[slot]; node; prev = node, node = node->hash_next)
	{
		if (node == theNode)
		{
			if (prev)
				prev->hash_next = node->hash_next;
			else
				_tab[slot] = node->hash_next;
			_total--;
			node->Node::~Node();
			free(node);
			return;
		}
	}
	assert(!"can't reach here");
}

StringHashSet::NodeType* StringHashSet::find(const char *key, size_t len) const
{
	Node *node;
	uint32_t ksum = jenkins_hash(key, len, 0);
	uint32_t slot = (ksum & _mask);

	for (node = _tab[slot]; node; node = node->hash_next)
	{
		if (node->ksum == ksum && node->klen == len && memcmp(node->key, key, len) == 0)
			return node;
	}
	return NULL;
}

StringHashSet::NodeType* StringHashSet::next(NodeType *node) const
{
	uint32_t slot = 0;
	if (node)
	{
		if (node->hash_next)
			return node->hash_next;
		else
		{
			slot = node->ksum & _mask;
			++slot;
			if (slot > _mask)
				return NULL;
		}
	}

	while (slot <= _mask)
	{
		if (_tab[slot])
			return _tab[slot];
		++slot;
	}

	return NULL;
}

StringHashSet::Node::Node(const char *key_, size_t len_): klen(len_)
{
	memcpy((char *)key, key_, klen);
	((char *)key)[klen] = '\0';
	ksum = jenkins_hash(key, klen, 0);
}

