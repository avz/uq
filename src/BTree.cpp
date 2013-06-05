#include "BTree.hpp"
#include "BTreeNode.hpp"

#include "blockStorage/Storage.hpp"
#include "blockStorage/backend/File.hpp"
#include "external/murmurhash3/MurmurHash3.h"

BTree::BTree(blockStorage::Storage<blockStorage::backend::File, BTreeNode, Superblock> &storage):
	storage(storage),
	nodeSize(64*1024),
	leafSize(4*1024)
{
};

bool BTree::addKey(uint64_t key) {
	uint64_t splitKey;
	uint64_t inserted = this->root->add(key, *this, &splitKey);

	if(!inserted)
		return false;

	if(inserted == 1)
		return true;

	/*
	 * ключ добавился, но рутовая нода сплитнулась - надо создавать новую
	 */
	BTreeNode *newRoot = this->allocateNode(this->root->level + 1);
	newRoot->makeNode(splitKey, this->root->id, inserted);

	this->superblock->setFirstBlock(newRoot->id, newRoot->level);

	this->root = newRoot;

	return true;
};

void BTree::init() {
	this->superblock = this->storage.getSuperblock();

	if(!this->superblock->getFirstBlockId()) {
		BTreeNode *root;
		root = this->allocateLeaf(0);

		this->superblock->setFirstBlock(root->id, 0);
	}

	uint32_t level = this->superblock->getRootLevel();
	uint64_t firstBlockId = this->superblock->getFirstBlockId();
//	fprintf(stderr, "%u, %lu\n", level, firstBlockId);
//	exit(1);
	if(level) {
		this->root = this->getNode(firstBlockId, level);
	} else {
		this->root = this->getLeaf(firstBlockId, level);
	}
}

bool BTree::add(void *buf, ssize_t size) {
	uint64_t b[2];

	MurmurHash3_x64_128(buf, size, 0, &b);

	b[0] ^= b[1];

	/* гарантируем, что не будет хеша 0, херово для коллизий, но никуда не деться */
	if(b[0] == 0)
		b[0] = 1;

	return this->addKey(b[0]);
}

/**
 * Этот метод вызывается из нод, если они хотят сплитнуться
 * @param argc
 * @param argv
 * @return
 */
BTreeNode *BTree::allocateLeaf(uint32_t level) {
	BTreeNode *n = this->storage.allocate(this->leafSize);
	n->makeLeaf();
	n->markAsDirty();
	n->level = level;

	return n;
}

BTreeNode *BTree::getLeaf(uint64_t id, uint32_t level) {
	BTreeNode *n = this->storage.get(id, this->leafSize);
	n->level = level;

	return n;
}

/**
 * Этот метод вызывается из нод, если они хотят сплитнуться
 * @param argc
 * @param argv
 * @return
 */
BTreeNode *BTree::allocateNode(uint32_t level) {
	BTreeNode *n = this->storage.allocate(this->nodeSize);
	n->makeNode();
	n->markAsDirty();
	n->level = level;

	return n;
}

BTreeNode *BTree::getNode(uint64_t id, uint32_t level) {
	BTreeNode *n = this->storage.get(id, this->nodeSize);
	n->level = level;

	return n;
}
