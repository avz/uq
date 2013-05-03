#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "storage.hpp"
#include "misc.hpp"

Block::Block(size_t blockSize) {
	this->id = 0xffffffff;
	this->refCount = 0;
	this->needUpdate = false;
	this->ptr = (char *)calloc(blockSize, 1);
}

Block::~Block() {
	::free(this->ptr);
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
	:fd(-1),
	blocksCacheSize(SIZE_T_MAX),
	superblock(NULL)
{
	this->filename = new char[strlen(filename)+1];
	strcpy(this->filename, filename);
	blocksCacheCurrentSize = 0;
	gcTimer = 1;
	gcProbability = 2048;
}

BlockStorage::~BlockStorage() {
	if(this->superblock)
		delete superblock;

	this->superblock = NULL;

	fprintf(stderr, "Flush cached blocks ...");
	this->setCacheSize(0);
	this->_gc(true);
	fprintf(stderr, " done\n");

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

	this->superblock = new BlockStorageSuperblock(this->get(0));

	if(this->superblock->blocksCount < this->prefetchSize)
		this->prefetchSize = this->superblock->blocksCount;

	if(this->blocksCacheSize < this->prefetchSize)
		this->prefetchSize = this->blocksCacheSize;

	fprintf(stderr, "Prefetch %u leading blocks ...", (unsigned int)this->prefetchSize);

	for(size_t i=0; i<this->prefetchSize; i++) {
		this->get(i)->free();
	}
	fprintf(stderr, " done\n");
}

Block *BlockStorage::allocate() {
	Block *b;
	this->_extendFile((this->superblock->blocksCount + 1) * this->blockSize);
	b = new Block(this->blockSize);
	b->refCount = 1;
	b->id = this->superblock->blocksCount;

// 	fprintf(stderr, "alloc %u\n", b->id);
	this->blocksCache.push_back(b);
	this->blocksCacheMap[b->id] = --this->blocksCache.end();
	this->blocksCacheCurrentSize++;

	this->superblock->blocksCount++;
	this->superblock->update();

	if(this->gcTimer % this->gcProbability == 0)
		this->_gc();
	this->gcTimer++;

	return b;
}

Block *BlockStorage::get(uint32_t id) {
// 	fprintf(stderr, "get %u\n", id);
	if(this->superblock && id >= this->superblock->blocksCount) {
		fprintf(stderr, "Requested block %u is out of range [0; %u)\n", id, this->superblock->blocksCount);
		exit(11);
	}

	Block *b;

	if(this->blocksCacheMap.find(id) == this->blocksCacheMap.end()) {
		if(this->gcTimer % this->gcProbability == 0)
			this->_gc();
		this->gcTimer++;

		b = new Block(this->blockSize);
		b->id = id;

		off_t offset = id * this->blockSize;
		if(offset < this->_fileSize()) {
			if((size_t)pread(this->fd, b->ptr, this->blockSize, offset) != this->blockSize) {
				perror("get:pread");
				fprintf(stderr, "Block id: %u\n", (unsigned int)id);
				exit(errno ? errno : 255);
			}
		}

		this->blocksCache.push_back(b);
		this->blocksCacheMap[id] = --this->blocksCache.end();
		++this->blocksCacheCurrentSize;
	} else {
		if(this->blocksCacheSize != SIZE_T_MAX) {
			b = *this->blocksCacheMap[id];
			this->blocksCache.erase(this->blocksCacheMap[id]);
			this->blocksCache.push_back(b);
			this->blocksCacheMap[id] = --this->blocksCache.end();
		}
	}

	(*this->blocksCacheMap[id])->refCount++;

	return *this->blocksCacheMap[id];
}

void BlockStorage::_extendFile(off_t newSize) {
#define PREALLOC_ENABLED
#ifdef PREALLOC_ENABLED
	ssize_t blockSize = 4096;
	off_t oldSize = this->_fileSize();
	if(oldSize < newSize) {
		char *zero;
		ssize_t i;
		newSize -= newSize % 1024*1024;
		newSize += 1024*1024;
		zero = (char *)alloca(blockSize);
		bzero(zero, blockSize);

		if(lseek(this->fd, 0, SEEK_END) < 0)
			fatal("lseek");

		for(i=0; i<(newSize - oldSize)/blockSize; i++) {
			if(write(this->fd, zero, blockSize) != blockSize)
				fatal("write");
		}

		if((newSize - oldSize) % blockSize) {
			if(write(this->fd, zero, (newSize - oldSize)%blockSize) != (newSize - oldSize)%blockSize)
				fatal("write");
		}
	}
#else
	if(this->_fileSize() < newSize) {
		newSize -= newSize % 1024*1024;
		newSize += 1024*1024;

		if(ftruncate(this->fd, newSize) < 0)
			fatal("truncate");
	}
#endif
}

off_t BlockStorage::_fileSize() {
	struct stat s;

	if(fstat(this->fd, &s) < 0) {
		perror("fstat");
		exit(errno);
	}

	return s.st_size;
}

static bool blockcmp(const Block * b1, const Block *b2) {
	return b1->id < b2->id;
}

void BlockStorage::_gc(bool cleanAll) {
// 	fprintf(stderr, "GC launch ... %u %u\n", this->blocksCacheCurrentSize, this->blocksCacheSize);
	if(this->blocksCacheCurrentSize < this->blocksCacheSize)
		return;

// 	fprintf(stderr, "GC launch ...");
	std::list<Block *>::iterator i = this->blocksCache.begin();
	std::vector<Block *> flushBuffer;
	size_t deleted = 0;

	/* 1 stage: delete clean blocks */
	while(i != this->blocksCache.end() && this->blocksCacheCurrentSize > this->blocksCacheSize) {
		Block *b = *i;
		if(!b->needUpdate && (!b->refCount || cleanAll)) {
			i = this->blocksCache.erase(i);
			this->blocksCacheMap.erase(b->id);
			--this->blocksCacheCurrentSize;
			deleted++;

			delete b;
		} else {
			++i;
		}
	}

	/* 2 stage: delete dirty blocks */
	i = this->blocksCache.begin();

	while(i != this->blocksCache.end() && this->blocksCacheCurrentSize > this->blocksCacheSize) {
		Block *b = *i;
		if(b->needUpdate && (!b->refCount || cleanAll)) {
			i = this->blocksCache.erase(i);
			this->blocksCacheMap.erase(b->id);
			--this->blocksCacheCurrentSize;
			deleted++;

			flushBuffer.push_back(b);
		} else {
			++i;
		}
	}

	if(flushBuffer.size()) {
// 		fprintf(stderr, "\nDropped: %u (flushed: %u)\n", (unsigned int)deleted, (unsigned int)flushBuffer.size());
		std::sort(flushBuffer.begin(), flushBuffer.end(), blockcmp);

		std::vector<Block *>::iterator vi;

		for(vi=flushBuffer.begin(); vi!=flushBuffer.end(); ++vi) {
			Block *b = *vi;
			pwrite(this->fd, b->ptr, this->blockSize, b->id * this->blockSize);
			delete b;
		}
// 		fprintf(stderr, "done\n");
	}
}

void BlockStorage::setCacheSize(size_t size) {
	this->blocksCacheSize = size;
}

void BlockStorage::setPrefetchSize(size_t size) {
	this->prefetchSize= size;
}
