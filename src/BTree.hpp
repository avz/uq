#ifndef BTREE_HPP
#define	BTREE_HPP

#include "BTreeNode.hpp"
#include "Superblock.hpp"
#include "blockStorage/backend/File.hpp"
#include "blockStorage/Storage.hpp"

class BTreeNode;

//template <typename StorageBackendT>
class BTree {
	blockStorage::Storage<blockStorage::backend::File, BTreeNode, Superblock> &storage;

	ssize_t nodeSize;
	ssize_t leafSize;

	Superblock *superblock;
	BTreeNode *root;

public:
	BTree(blockStorage::Storage<blockStorage::backend::File, BTreeNode, Superblock> &storage);

	bool addKey(uint64_t key);

	bool add(void *buf, ssize_t size);

	void init();

	/**
	 * Этот метод вызывается из нод, если они хотят сплитнуться
     * @param argc
     * @param argv
     * @return
     */
	BTreeNode *allocateNode(uint32_t level);
	BTreeNode *getNode(uint64_t id, uint32_t level);

	BTreeNode *allocateLeaf(uint32_t level);
	BTreeNode *getLeaf(uint64_t id, uint32_t level);
};

#endif	/* BTREE_HPP */

