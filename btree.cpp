#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>

#include "storage.hpp"
#include "btree_node.hpp"
#include "btree.hpp"

UniqueBTreeSuperblock::UniqueBTreeSuperblock(void *ptr)
	:keySize(*(uint32_t *)ptr),
	rootNodeId(*((uint32_t *)ptr + 1))
{}


UniqueBTree::UniqueBTree(const char *filename)
	:BlockStorage(filename),
	root(NULL),
	keysAddedInCurrentSession(0),
	superblock(NULL)
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
}

void UniqueBTree::load() {
	BlockStorage::load();
	this->reload();
}

void UniqueBTree::reload() {
	if(this->superblock)
		delete this->superblock;

	this->superblock = new UniqueBTreeSuperblock(this->get(1).ptr);

	this->blockSize = BlockStorage::superblock->blockSize;
	this->keySize = this->superblock->keySize;

	if(this->root)
		delete this->root;

	this->root = new UniqueBTreeNode(this, this->get(this->superblock->rootNodeId));
}

bool UniqueBTree::add(void *key) {
	this->keysAddedInCurrentSession++;
	if(this->keysAddedInCurrentSession > 1000000 || this->needRemap) {
		this->remap();
		this->keysAddedInCurrentSession = 0;
	}
// fprintf(stderr, "root size: %u\n", this->root->numKeys);
// fprintf(stderr, "key size: %u\n", this->keySize);
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
		}
	} while(true);
}

void UniqueBTree::remap() {
// 	fprintf(stderr, "Remmaping\n");
	this->flush();
	BlockStorage::remap();
}


void UniqueBTree::onRemap() {
	if(!this->superblock) { // creating
		Block sb = this->allocate();
		this->root = new UniqueBTreeNode(this, this->allocate());
		this->root->isLeaf = 1;
		this->root->numKeys = 0;

		this->superblock = new UniqueBTreeSuperblock(sb.ptr);
		this->superblock->keySize = 8;
		this->superblock->rootNodeId = root->blockId;

		this->blockSize = BlockStorage::superblock->blockSize;
		this->keySize = this->superblock->keySize;
	} else { // loading and remapping
		this->reload();
	}
}
