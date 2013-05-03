#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>

#include "storage.hpp"
#include "btree_node.hpp"
#include "btree.hpp"

UniqueBTreeSuperblock::UniqueBTreeSuperblock(Block *block)
	:block(block),
	keySize(*(uint32_t *)block->ptr),
	rootNodeId(*((uint32_t *)block->ptr + 1))
{}

UniqueBTreeSuperblock::~UniqueBTreeSuperblock() {
	this->block->free();
}

void UniqueBTreeSuperblock::update() {
	this->block->update();
}


UniqueBTree::UniqueBTree(const char *filename)
	:BlockStorage(filename),
	root(NULL),
	superblock(NULL),
	keySize(8),
	blockSize(4096*2)
{
}

UniqueBTree::~UniqueBTree() {
	if(superblock)
		delete superblock;

	if(root)
		delete root;
}

void UniqueBTree::create(size_t blockSize) {
	BlockStorage::create(blockSize);

	this->superblock = new UniqueBTreeSuperblock(this->allocate());
	this->keySize = this->superblock->keySize = 8;

	this->root = new UniqueBTreeNode(this, this->allocate());
	this->root->isLeaf = 1;
	this->root->numKeys = 0;

	this->superblock->rootNodeId = root->blockId;

	this->superblock->update();
	this->root->update();

	this->blockSize = BlockStorage::superblock->blockSize;
}

void UniqueBTree::load() {
	BlockStorage::load();
	this->reload();
}

void UniqueBTree::reload() {
	if(this->superblock)
		delete this->superblock;

	this->superblock = new UniqueBTreeSuperblock(BlockStorage::get(1));

	this->blockSize = BlockStorage::superblock->blockSize;
	this->keySize = this->superblock->keySize;

	if(this->root)
		delete this->root;

	this->root = new UniqueBTreeNode(this, BlockStorage::get(this->superblock->rootNodeId));
}

bool UniqueBTree::add(const void *key) {
	do {
		try {
			return this->root->add(key);
		} catch(UniqueBTreeNode_NeedSplit e) {
			UniqueBTreeNode *newRoot = new UniqueBTreeNode(this, this->allocate());
			UniqueBTreeNode newNode(this, this->allocate());
			char newKey[this->keySize];

			this->root->split(&newNode, newKey);
			newRoot->_appendKey(newKey);
			newRoot->numKeys = 1;
			newRoot->isLeaf = false;

			newRoot->childs[0] = this->root->blockId;
			newRoot->childs[1] = newNode.blockId;

			delete this->root;
			this->root = newRoot;
			this->superblock->rootNodeId = newRoot->blockId;
			newRoot->update();
			this->superblock->update();
		}
	} while(true);
}

UniqueBTreeNode *UniqueBTree::get(uint32_t id) {
	return new UniqueBTreeNode(this, BlockStorage::get(id));
}
