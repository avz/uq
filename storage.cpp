#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>

#include "storage.hpp"

BlockStorageSuperblock::BlockStorageSuperblock(void *ptr)
	:blockSize(*(uint32_t *)ptr),
	blocksCount(*(((uint32_t *)ptr) + 1))
{
}

/* -- */

BlockStorage::BlockStorage(const char *filename)
	:fd(-1), mmapedLen(1*1024*1024*1024), superblock(NULL)
{
	this->filename = new char[strlen(filename)+1];
	strcpy(this->filename, filename);
}

BlockStorage::~BlockStorage() {
	delete[] this->filename;

	if(this->map)
		munmap(this->map, this->mmapedLen);

	this->map = NULL;

	if(this->fd >= 0)
		close(this->fd);

	this->fd = -1;

	if(this->superblock)
		delete superblock;
	this->superblock = NULL;
}

void BlockStorage::create(size_t blockSize) {
	this->fd = open(this->filename, O_RDWR | O_CREAT | O_TRUNC, 0640);
	if(this->fd < 0) {
		perror(this->filename);
		exit(errno);
	}

	this->_doMmap();
	this->_extend(blockSize);

	this->superblock = new BlockStorageSuperblock(this->map);
	this->superblock->blockSize = blockSize;
	this->superblock->blocksCount = 1;
}

void BlockStorage::load() {
	this->fd = open(this->filename, O_RDWR);
	if(this->fd < 0) {
		perror(this->filename);
		exit(errno);
	}
	this->_doMmap();

	this->superblock = new BlockStorageSuperblock(this->map);
}

Block BlockStorage::allocate() {
	Block b;
	this->_extend((this->superblock->blocksCount + 1) * this->superblock->blockSize);

	b.id = this->superblock->blocksCount;
	b.ptr = (char *)this->map + b.id * this->superblock->blockSize;

	this->superblock->blocksCount++;

	return b;
}

Block BlockStorage::get(uint32_t id) {
	if(id >= this->superblock->blocksCount) {
		fprintf(stderr, "Requested block %u is out of range [0; %u)\n", id, this->superblock->blocksCount);
		exit(11);
	}

	Block b;
	b.id = id;
	b.ptr = (char *)this->map + id * this->superblock->blockSize;

	return b;
}

void BlockStorage::_extend(size_t newSize) {
	if(ftruncate(this->fd, newSize) < 0) {
		perror("truncate");
		exit(errno);
	}
}

void BlockStorage::_doMmap() {
	this->map = mmap(NULL, this->mmapedLen, PROT_READ | PROT_WRITE, MAP_SHARED/* | MAP_NOCORE*/ /*| MAP_NOSYNC*/, fd, 0);
	if(this->map == MAP_FAILED) {
		perror("mmap");
		exit(errno);
	}
}

void BlockStorage::flush() {
	if(fsync(this->fd) < 0) {
		perror("fsync");
		exit(errno);
	}
}

void BlockStorage::remap() {
	munmap(this->map, this->mmapedLen);
	this->_doMmap();
}
