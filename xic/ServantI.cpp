#include "ServantI.h"
#include "xslib/bit.h"
#include "xslib/jenkins.h"
#include <limits.h>

using namespace xic;


MethodTab::NodeType::NodeType(const char *name_, size_t nlen_, const Servant::MethodFunction& func_)
	: func(func_), nlen(nlen_)
{
	memcpy(name, name_, nlen);
	name[nlen] = 0;
	hash = jenkins_hash(name, nlen, 0);
}


MethodTab::MethodTab(const PairType* pairs, size_t size)
{
	size_t slot_num = XS_CLAMP(size, 16, SHRT_MAX);
	slot_num = round_up_power_two(slot_num * 2);
	_ostk = ostk_create(0);
	_tab = OSTK_CALLOC(_ostk, NodeType*, slot_num);
	_mask = slot_num - 1;
	_total = 0;

	for (size_t i = 0; i < size; ++i)
	{
		insert(pairs[i].name, strlen(pairs[i].name), pairs[i].func);
	}
}

MethodTab::~MethodTab()
{
	for (uint32_t slot = 0; slot <= _mask; ++slot)
	{
		NodeType *node, *next;
		for (node = _tab[slot]; node; node = next)
		{
			next = node->hash_next;
			node->NodeType::~NodeType();
		}
	}
	ostk_destroy(_ostk);
}

MethodTab::NodeType* MethodTab::insert(const char *name, size_t len, const Servant::MethodFunction& func)
{
	NodeType *node;
	uint32_t hash = jenkins_hash(name, len, 0);
	uint32_t slot = (hash & _mask);

	for (node = _tab[slot]; node; node = node->hash_next)
	{
		if (node->hash == hash && node->nlen == len && memcmp(node->name, name, len) == 0)
			return NULL;
	}

	void *p = ostk_alloc(_ostk, sizeof(NodeType) + len + 1);
	node = new(p) NodeType(name, len, func);
	node->hash_next = _tab[slot];
	_tab[slot] = node;
	_total++;
	return node;
}

MethodTab::NodeType* MethodTab::find(const xstr_t& key) const
{
	NodeType *node;
	uint32_t hash = jenkins_hash(key.data, key.len, 0);
	uint32_t slot = (hash & _mask);
	for (node = _tab[slot]; node; node = node->hash_next)
	{
		if (node->hash == hash && node->nlen == key.len && memcmp(node->name, key.data, key.len) == 0)
			return node;
	}
	return NULL;
}

MethodTab::NodeType* MethodTab::next(const NodeType *node) const
{
	uint32_t slot = 0;
	if (node)
	{
		if (node->hash_next)
			return node->hash_next;
		else
		{
			slot = node->hash & _mask;
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

void MethodTab::mark(const xstr_t& method, bool on) const
{
	NodeType* node = find(method);
	if (node)
		node->mark = on;
}

