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


UniqueBTree::UniqueBTree(const char *filename, bool readOnlyMode):
	root(NULL),
	storage(filename, readOnlyMode),
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
	this->storage.create(blockSize);

	this->superblock = new UniqueBTreeSuperblock(this->storage.allocate());
	this->superblock->keySize = this->keySize;

	this->root = new UniqueBTreeNode(this, this->storage.allocate());
	this->root->isLeaf = 1;
	this->root->numKeys = 0;

	this->superblock->rootNodeId = root->blockId;

	this->superblock->update();
	this->root->update();

	this->blockSize = this->storage.superblock->blockSize;
}

void UniqueBTree::load() {
	this->storage.load();
	this->reload();
}

void UniqueBTree::reload() {
	if(this->superblock)
		delete this->superblock;

	this->superblock = new UniqueBTreeSuperblock(this->storage.get(1));

	this->blockSize = this->storage.superblock->blockSize;
	this->keySize = this->superblock->keySize;

	if(this->root)
		delete this->root;

	this->root = new UniqueBTreeNode(this, this->storage.get(this->superblock->rootNodeId));
}

bool UniqueBTree::add(const void *key) {
	do {
		try {
			return this->root->add(key);
		} catch(UniqueBTreeNode_NeedSplit e) {
			UniqueBTreeNode *newRoot = new UniqueBTreeNode(this, this->storage.allocate());
			UniqueBTreeNode newNode(this, this->storage.allocate());
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

bool UniqueBTree::check(const void *key) {
	return this->root->isNotExists(key);
}

void UniqueBTree::setKeySize(unsigned char size) {
	this->keySize = size;
}

UniqueBTreeNode *UniqueBTree::get(uint32_t id) {
	return new UniqueBTreeNode(this, this->storage.get(id));
}
