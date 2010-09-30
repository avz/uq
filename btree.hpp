#ifndef BTREE_HPP
#define BTREE_HPP
#include <sys/types.h>
#include <stdint.h>
#include "btree_node.hpp"

class UniqueBTreeNode;

class UniqueBTreeNode_NeedSplit {};

class UniqueBTreeSuperblock {
public:
	uint32_t &keySize;
	uint32_t &rootNodeId;
	UniqueBTreeSuperblock(void *ptr);
};

class UniqueBTree {
	UniqueBTreeNode *root;
	unsigned int keysAddedInCurrentSession;
public:
	BlockStorage storage;
	UniqueBTreeSuperblock *superblock;
	uint32_t keySize;
	uint32_t blockSize;
	UniqueBTree(const char *filename);
// 	~UniqueBTree();

	void create();
	void load();
	void reload();
	bool add(void *key);

	void remap();
};

#endif
