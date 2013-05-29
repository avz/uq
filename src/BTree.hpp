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

public:
	BTree(blockStorage::Storage<blockStorage::backend::File, BTreeNode, Superblock> &storage);

	bool addKey(uint64_t key);

	bool add(void *buf, ssize_t size);

	/**
	 * Этот метод вызывается из нод, если они хотят сплитнуться
     * @param argc
     * @param argv
     * @return
     */
	BTreeNode *allocate();

	BTreeNode *get(uint64_t id);
};

#endif	/* BTREE_HPP */

