#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "misc.hpp"
#include "btree_node.hpp"

UniqueBTreeNode::UniqueBTreeNode(UniqueBTree *tree, Block bl)
	:tree(tree),
	keys((uint32_t *)bl.ptr + 2),
	childs(NULL),
	isLeaf(*(uint32_t *)bl.ptr),
	numKeys(*((uint32_t *)bl.ptr + 1)),
	blockId(bl.id)
{
	this->childs = (uint32_t *)((char*)this->keys + this->maxKeys() * this->tree->keySize);
}

void UniqueBTreeNode::split(UniqueBTreeNode *right, void *key) {
// 	fprintf(stderr, "%u / %u\n", numKeys, maxKeys());
	uint32_t median = this->numKeys / 2;
	char *t = (char *)this->keys + median * this->tree->keySize;

	memcpy(key, t, this->tree->keySize);
	memcpy(right->keys, t + this->tree->keySize, (this->numKeys - median - 1) * this->tree->keySize);
	right->numKeys = this->numKeys - median - 1;
	this->numKeys = median;

	right->isLeaf = this->isLeaf;

	if(!this->isLeaf) {
		memcpy(right->childs, this->childs + median + 1, (right->numKeys + 1) * sizeof(*right->childs));
	}
}

bool UniqueBTreeNode::add(void *key) {
	off_t t;

	if(this->isLeaf) {
		if(this->numKeys >= this->maxKeys()) {
			throw UniqueBTreeNode_NeedSplit();
		}

		if(this->numKeys) {
			t = insertInSortedArray(this->keys, this->tree->keySize, this->numKeys, key);
			if(t >= 0)
				this->numKeys++;

			return t >= 0;
		} else {
			memcpy(this->keys, key, this->tree->keySize);
			this->numKeys = 1;
		}
	} else {
		do {
			t = searchInterval(this->keys, this->tree->keySize, this->numKeys, key);
			if(t == -1)
				return false;
			UniqueBTreeNode n(this->tree, this->tree->storage.get(this->childs[t]));

			try {
				return n.add(key);
			} catch(UniqueBTreeNode_NeedSplit e) {
				if(this->numKeys >= this->maxKeys())
					throw UniqueBTreeNode_NeedSplit();

				UniqueBTreeNode right(this->tree, this->tree->storage.allocate());
				char newKey[this->tree->keySize];
				n.split(&right, newKey);

				insertInArray(this->keys, this->tree->keySize, this->numKeys, newKey, t);
				this->numKeys++;

				insertInArray(this->childs, sizeof(uint32_t), this->numKeys, &right.blockId, t + 1);
			}
		} while(true);
	}

	return true;
}

uint32_t UniqueBTreeNode::maxKeys() {
	if(this->isLeaf)
		return (this->tree->blockSize - sizeof(uint32_t) * 2) / this->tree->keySize;
	else
		return (this->tree->blockSize - sizeof(uint32_t) * 2 - sizeof(uint32_t)) / (this->tree->keySize + sizeof(uint32_t));
}

void UniqueBTreeNode::_appendKey(void *key) {
	memcpy((char *)this->keys + this->numKeys * this->tree->keySize, key, this->tree->keySize);
	this->numKeys++;
}

