#include <stdio.h>
#include <string.h>
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
	this->setIsLeaf(this->isLeaf);
}

void UniqueBTreeNode::setIsLeaf(bool isLeaf) {
	this->isLeaf = (uint32_t)isLeaf;
	if(!isLeaf)
		this->childs = (uint32_t *)((char *)this->keys + this->numKeys * this->tree->keySize);
}

void UniqueBTreeNode::split(UniqueBTreeNode *right, void *key) {
	uint32_t median = this->numKeys / 2;
	char *t = (char *)this->keys + median * this->tree->keySize;

	memcpy(key, t, this->tree->keySize);
	memcpy(right->keys, t + this->tree->keySize, (this->numKeys - median - 1) * this->tree->keySize);
	right->numKeys = this->numKeys - median - 1;
	this->numKeys = median;

	right->setIsLeaf(this->isLeaf);

	if(!this->isLeaf) {
		memcpy(right->childs, this->childs + median + 1, (right->numKeys + 1) * sizeof(*right->childs));
		t = (char *)this->keys + this->numKeys * this->tree->keySize;
		memmove(t, this->childs, (this->numKeys + 1) * this->tree->keySize);
		this->setIsLeaf(this->isLeaf);
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
/*if(this->childs[t] > 100) {
	fprintf(stderr, "id: %u, numKeys: %u\n", this->blockId, this->numKeys);
}*/
			UniqueBTreeNode n(this->tree, this->tree->storage.get(this->childs[t]));

			try {
				return n.add(key);
			} catch(UniqueBTreeNode_NeedSplit e) {
				if(this->numKeys >= this->maxKeys())
					throw UniqueBTreeNode_NeedSplit();

				UniqueBTreeNode right(this->tree, this->tree->storage.allocate());
				char newKey[this->tree->keySize];
				n.split(&right, newKey);

				uint64_t zero = 0x3d3d3d3d3d3d3d3d;
				this->_appendKey(&zero); // fake, to shift childs
				insertInArray(this->keys, this->tree->keySize, this->numKeys-1, newKey, t);
				this->setIsLeaf(this->isLeaf); //!

// printf("b: %u; num keys: %u\n", *(uint32_t *)this->tree->storage.get(this->blockId + 1).ptr, this->numKeys);
				insertInArray(this->childs, sizeof(uint32_t), this->numKeys, &right.blockId, t + 1);
// printf("a: %u\n", *(uint32_t *)this->tree->storage.get(this->blockId + 1).ptr);
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
	if(!this->isLeaf) {
		memmove((char *)this->childs + this->tree->keySize, this->childs, (this->numKeys + 1) * sizeof(uint32_t));
		this->childs = (uint32_t *)((char *)this->childs + this->tree->keySize);
	}
	memcpy((char *)this->keys + this->numKeys * this->tree->keySize, key, this->tree->keySize);
	this->numKeys++;
}

