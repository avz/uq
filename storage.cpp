#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#include "storage.hpp"

Block::Block(size_t blockSize) {
	this->id = 0xffffffff;
	this->refCount = 0;
	this->needUpdate = true;
	this->ptr = new char[blockSize];
	bzero(this->ptr, blockSize);
}

Block::~Block() {
	delete[] this->ptr;
}

void Block::update() {
	this->needUpdate = true;
}

void Block::free() {
	this->refCount--;
}

BlockStorageSuperblock::BlockStorageSuperblock(Block *block)
	:block(block),
	blockSize(*(uint32_t *)block->ptr),
	blocksCount(*(((uint32_t *)block->ptr) + 1))
{
}

BlockStorageSuperblock::~BlockStorageSuperblock() {
	this->block->free();
}

void BlockStorageSuperblock::update() {
	this->block->update();
}

/* -- */

BlockStorage::BlockStorage(const char *filename)
	:fd(-1), superblock(NULL)
{
	this->filename = new char[strlen(filename)+1];
	strcpy(this->filename, filename);
}

BlockStorage::~BlockStorage() {
	if(this->superblock)
		delete superblock;

	this->superblock = NULL;

	this->_gc();

	delete[] this->filename;

	if(this->fd >= 0)
		close(this->fd);

	this->fd = -1;
}

void BlockStorage::create(size_t blockSize) {
	this->blockSize = blockSize;

	this->fd = open(this->filename, O_RDWR | O_CREAT | O_TRUNC, 0640);
	if(this->fd < 0) {
		perror(this->filename);
		exit(errno);
	}

	this->_extendFile(blockSize);

	this->superblock = new BlockStorageSuperblock(this->get(0));
	this->superblock->blockSize = blockSize;
	this->superblock->blocksCount = 1;
	this->superblock->update();
}

void BlockStorage::load() {
	this->fd = open(this->filename, O_RDWR);
	if(this->fd < 0) {
		perror(this->filename);
		exit(errno);
	}

	if(pread(this->fd, &this->blockSize, sizeof(this->blockSize), 0) != sizeof(this->blockSize)) {
		perror("load:pread");
		exit(errno);
	}
// 	fprintf(stderr, "block size: %u\n", this->blockSize);
// 	exit(255);

	this->superblock = new BlockStorageSuperblock(this->get(0));
}

Block *BlockStorage::allocate() {
	Block *b;
	this->_extendFile((this->superblock->blocksCount + 1) * this->blockSize);
	b = new Block(this->blockSize);
	b->refCount = 1;
	b->id = this->superblock->blocksCount;

// 	fprintf(stderr, "alloc %u\n", b->id);
	this->blocksCache[b->id] = b;

	this->superblock->blocksCount++;
	this->superblock->update();

	return b;
}

Block *BlockStorage::get(uint32_t id) {
// 	fprintf(stderr, "get %u\n", id);
	if(this->superblock && id >= this->superblock->blocksCount) {
		fprintf(stderr, "Requested block %u is out of range [0; %u)\n", id, this->superblock->blocksCount);
		exit(11);
	}

// 	this->_gc();

	if(this->blocksCache.find(id) == this->blocksCache.end()) {
		Block *b = new Block(this->blockSize);
		b->id = id;

		off_t offset = id * this->blockSize;
		if(offset < this->_fileSize()) {
			if((size_t)pread(this->fd, b->ptr, this->blockSize, offset) != this->blockSize) {
				perror("get:pread");
				fprintf(stderr, "Block id: %u\n", (unsigned int)id);
				exit(errno ? errno : 255);
			}
		}
		this->blocksCache[id] = b;
	}

	this->blocksCache[id]->refCount++;

	return this->blocksCache[id];
}

void BlockStorage::_extendFile(off_t newSize) {
	if(this->_fileSize() < newSize) {
		newSize -= newSize % 1024*1024;
		newSize += 1024*1024;

		if(ftruncate(this->fd, newSize) < 0) {
			perror("truncate");
			exit(errno);
		}
	}
}

off_t BlockStorage::_fileSize() {
	struct stat s;

	if(fstat(this->fd, &s) < 0) {
		perror("fstat");
		exit(errno);
	}

	return s.st_size;
}

void BlockStorage::_gc() {
	std::map<uint32_t, Block *>::iterator i = this->blocksCache.begin();

	while(i != this->blocksCache.end()) {
// 		fprintf(stderr, "gc check %u\n", (unsigned int)i->second->id);
		if(!i->second->refCount) {
			if(i->second->needUpdate) {
// 				fprintf(stderr, "writing %u\n", (unsigned int)i->second->id);
				pwrite(this->fd, i->second->ptr, this->blockSize, i->second->id * this->blockSize);
			}
			delete i->second;
			this->blocksCache.erase(i++);
		} else {
			++i;
		}
	}
/*
	i = this->blocksCache.begin();
	while(i != this->blocksCache.end()) {
		fprintf(stderr, "reachable %u [%u]\n", (unsigned int)i->second->id, (unsigned int)i->second->refCount);
		++i;
	}*/
}

