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
	:root(NULL),
	keysAddedInCurrentSession(0),
	storage(filename),
	superblock(NULL)
{
}

void UniqueBTree::create() {
	this->storage.create(4096*4);

	Block sb = this->storage.allocate();
	this->root = new UniqueBTreeNode(this, this->storage.allocate());
	this->root->isLeaf = 1;
	this->root->numKeys = 0;

	this->superblock = new UniqueBTreeSuperblock(sb.ptr);
	this->superblock->keySize = 8;
	this->superblock->rootNodeId = root->blockId;

	this->blockSize = this->storage.superblock->blockSize;
	this->keySize = this->superblock->keySize;
}

void UniqueBTree::load() {
	this->storage.load();
	this->reload();
}

void UniqueBTree::reload() {
	if(this->superblock)
		delete this->superblock;

	this->superblock = new UniqueBTreeSuperblock(this->storage.get(1).ptr);

	this->blockSize = this->storage.superblock->blockSize;
	this->keySize = this->superblock->keySize;

	if(this->root)
		delete this->root;

	this->root = new UniqueBTreeNode(this, this->storage.get(this->superblock->rootNodeId));
}

bool UniqueBTree::add(void *key) {
	this->keysAddedInCurrentSession++;
	if(this->keysAddedInCurrentSession > 2000000) {
		this->remap();
		this->keysAddedInCurrentSession = 0;
	}

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

			newRoot->setIsLeaf(false);

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
	this->storage.flush();
	this->storage.remap();
}
