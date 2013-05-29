#include "BTree.hpp"
#include "BTreeNode.hpp"

#include "blockStorage/Storage.hpp"
#include "blockStorage/backend/File.hpp"
#include "external/murmurhash3/MurmurHash3.h"

BTree::BTree(blockStorage::Storage<blockStorage::backend::File, BTreeNode, Superblock> &storage):
	storage(storage)
{
};

bool BTree::addKey(uint64_t key) {
	BTreeNode *root = this->storage.get(this->storage.getSuperblock()->getFirstBlockId());
	uint64_t splitKey;
	uint64_t inserted = root->add(key, *this, &splitKey);

	if(!inserted)
		return false;

	if(inserted == 1)
		return true;

	/*
	 * ключ добавился, но рутовая нода сплитнулась - надо создавать новую
	 */
	BTreeNode *newRoot = this->allocate();
	newRoot->makeNode(splitKey, root->id, inserted);

	this->storage.getSuperblock()->setFirstBlockId(newRoot->id);

	return true;
};

bool BTree::add(void *buf, ssize_t size) {
	uint64_t b[2];

	MurmurHash3_x64_128(buf, size, 0, &b);

	return this->addKey(b[0] ^ b[1]);
}

/**
 * Этот метод вызывается из нод, если они хотят сплитнуться
 * @param argc
 * @param argv
 * @return
 */
BTreeNode *BTree::allocate() {
	return this->storage.allocate();
}

BTreeNode *BTree::get(uint64_t id) {
	return this->storage.get(id);
}
