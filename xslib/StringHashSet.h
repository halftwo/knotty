/* $Id: StringHashSet.h,v 1.4 2012/01/30 06:55:30 jiagui Exp $ */
#ifndef StringHashSet_h_
#define StringHashSet_h_

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include "xstr.h"
#include "bit.h"
#include "jenkins.h"
#include <stdint.h>
#include <string>


class StringHashSet
{
	class Node;
public:
	typedef Node NodeType;

	StringHashSet(size_t size);
	~StringHashSet();

	NodeType* insert(const char *key, size_t len);
	NodeType* insert(const xstr_t& key) { return insert((const char *)key.data, key.len); }
	NodeType* insert(const std::string& key) { return insert(key.data(), key.length()); }

	bool remove(const char *key, size_t len);
	bool remove(const xstr_t& key) { return remove((const char *)key.data, key.len); }
	bool remove(const std::string& key) { return remove(key.data(), key.length()); }

	void remove(NodeType *node);

	NodeType* find(const char *key, size_t len) const;
	NodeType* find(const xstr_t& key) const { return find((const char *)key.data, key.len); }
	NodeType* find(const std::string& key) const { return find(key.data(), key.length()); }

	NodeType* next(NodeType *node) const;

private:
	class Node
	{
		friend class StringHashSet;
		Node(const char *key_, size_t len_);

		Node* hash_next;
	public:
		uint32_t ksum;
		uint32_t klen;
		char key[];
	};

	Node **_tab;
	unsigned int _mask;
	unsigned int _total;
};


#endif
