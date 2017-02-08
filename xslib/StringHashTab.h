/* $Id: StringHashTab.h,v 1.4 2011/07/20 01:53:03 jiagui Exp $ */
#ifndef StringHashTab_h_
#define StringHashTab_h_

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include "xstr.h"
#include "bit.h"
#include "jenkins.h"
#include <stdint.h>
#include <assert.h>
#include <string>


template<typename ValueType>
class StringHashTab
{
	class Node;
public:
	typedef Node NodeType;

	StringHashTab(size_t size);
	~StringHashTab();

	NodeType* insert(const char *key, size_t len);
	NodeType* insert(const xstr_t& key) { return insert(key.data, key.len); }
	NodeType* insert(const std::string& key) { return insert(key.data(), key.length()); }

	NodeType* insert(const char *key, size_t len, const ValueType& value);
	NodeType* insert(const xstr_t& key, const ValueType& value) { return insert(key.data, key.len, value); }
	NodeType* insert(const std::string& key, const ValueType& value) { return insert(key.data(), key.length(), value); }

	bool remove(const char *key, size_t len);
	bool remove(const xstr_t& key) { return remove(key.data, key.len); }
	bool remove(const std::string& key) { return remove(key.data(), key.length()); }

	void remove(NodeType *node);

	NodeType* find(const char *key, size_t len) const;
	NodeType* find(const xstr_t& key) const { return find(key.data, key.len); }
	NodeType* find(const std::string& key) const { return find(key.data(), key.length()); }

	NodeType* next(NodeType *node) const;

private:
	class Node
	{
		friend class StringHashTab;
		Node(const char *key_, size_t len_);
		Node(const char *key_, size_t len_, const ValueType& value_);

		Node* hash_next;
	public:
		ValueType value;
		uint32_t ksum;
		uint32_t klen;
		char key[];
	};

	Node **_tab;
	unsigned int _mask;
	unsigned int _total;
};

template<typename ValueType>
StringHashTab<ValueType>::StringHashTab(size_t size)
{
	size = round_up_power_two(size > INT_MAX ? INT_MAX: size < 16 ? 16 : size);
	_tab = (Node **)calloc(size, sizeof(*_tab));
	_mask = size - 1;
	_total = 0;
}

template<typename ValueType>
StringHashTab<ValueType>::~StringHashTab()
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

template<typename ValueType>
typename StringHashTab<ValueType>::NodeType*
StringHashTab<ValueType>::insert(const char *key, size_t len)
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

template<typename ValueType>
typename StringHashTab<ValueType>::NodeType*
StringHashTab<ValueType>::insert(const char *key, size_t len, const ValueType& value)
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
	node = new(p) Node(key, len, value);
	node->hash_next = _tab[slot];
	_tab[slot] = node;
	_total++;
	return node;
}

template<typename ValueType>
bool
StringHashTab<ValueType>::remove(const char *key, size_t len)
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

template<typename ValueType>
void
StringHashTab<ValueType>::remove(NodeType* theNode)
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

template<typename ValueType>
typename StringHashTab<ValueType>::NodeType*
StringHashTab<ValueType>::find(const char *key, size_t len) const
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

template<typename ValueType>
typename StringHashTab<ValueType>::NodeType*
StringHashTab<ValueType>::next(NodeType *node) const
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

template<typename ValueType>
StringHashTab<ValueType>::Node::Node(const char *key_, size_t len_): klen(len_)
{
	memcpy((char *)key, key_, klen);
	((char *)key)[klen] = '\0';
	ksum = jenkins_hash(key, klen, 0);
}

template<typename ValueType>
StringHashTab<ValueType>::Node::Node(const char *key_, size_t len_, const ValueType& value_): value(value_), klen(len_)
{
	memcpy((char *)key, key_, klen);
	((char *)key)[klen] = '\0';
	ksum = jenkins_hash(key, klen, 0);
}

#endif
