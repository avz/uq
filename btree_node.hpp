#ifndef BTREE_NODE_HPP
#define BTREE_NODE_HPP
#include <stdint.h>

#include "storage.hpp"
#include "btree.hpp"

class UniqueBTree;

class UniqueBTreeNode {
	Block *block;
	UniqueBTree *tree;

	uint32_t maxKeys();
public:
	void *keys;
	uint32_t *childs;

	uint32_t &isLeaf;
	uint32_t &numKeys;
	uint32_t blockId;

	UniqueBTreeNode(UniqueBTree *tree, Block *bl);
	~UniqueBTreeNode();

	void split(UniqueBTreeNode *right, void *key);
	bool add(const void *key);

	void mlock();

	void update();

	void _appendKey(void *key);
};

#endif
