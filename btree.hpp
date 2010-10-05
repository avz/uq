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

class UniqueBTree: public BlockStorage {
	UniqueBTreeNode *root;
	unsigned int keysAddedInCurrentSession;
public:
	UniqueBTreeSuperblock *superblock;
	uint32_t keySize;
	uint32_t blockSize;
	UniqueBTree(const char *filename);
	~UniqueBTree();

	void create(size_t blockSize);
	void load();
	void reload();
	bool add(const void *key);
	UniqueBTreeNode get(uint32_t id);

	void remap();

	void onRemap();
};

#endif
