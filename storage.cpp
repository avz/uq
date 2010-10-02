#include <sys/types.h>
#include <sys/stat.h>
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
	:fd(-1), map(NULL), needRemap(false), superblock(NULL)
{
	this->filename = new char[strlen(filename)+1];
	strcpy(this->filename, filename);
}

BlockStorage::~BlockStorage() {
	delete[] this->filename;

	this->unmap();

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

	this->_extendFile(blockSize);
	this->mmap(1024);

	this->superblock = new BlockStorageSuperblock(this->map);
	this->superblock->blockSize = blockSize;
	this->superblock->blocksCount = 1;

	this->remap();
}

void BlockStorage::load() {
	this->fd = open(this->filename, O_RDWR);
	if(this->fd < 0) {
		perror(this->filename);
		exit(errno);
	}

	this->remap();
}

Block BlockStorage::allocate() {
	Block b;
	this->_extendFile((this->superblock->blocksCount + 1) * this->superblock->blockSize);

	if((this->superblock->blocksCount + 5) * this->superblock->blockSize > this->mapSize)
		this->needRemap = true;

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

void BlockStorage::_extendFile(size_t newSize) {
	if(ftruncate(this->fd, newSize) < 0) {
		perror("truncate");
		exit(errno);
	}
}

void BlockStorage::mmap(size_t mapSize) {
	this->map = ::mmap(NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED/* | MAP_NOCORE*/ /*| MAP_NOSYNC*/, fd, 0);
	if(this->map == MAP_FAILED) {
		perror("mmap");
		exit(errno);
	}
// 	fprintf(stderr, "Map size: %llu\n", (unsigned long long)mapSize);
	this->mapSize = mapSize;
}

void BlockStorage::flush() {
	if(fsync(this->fd) < 0) {
		perror("fsync");
		exit(errno);
	}
}

void BlockStorage::unmap() {
	if(this->map)
		munmap(this->map, this->mapSize);

	this->map = NULL;
}

void BlockStorage::remap() {
	size_t mapSize;
	if(this->superblock)
		mapSize = this->_fileSize() + this->superblock->blockSize * 200;
	else
		mapSize = this->_fileSize() + 1024*1024*4;

	this->needRemap = false;
	this->unmap();
	this->mmap(mapSize);
	this->superblock = new BlockStorageSuperblock(this->map);
	this->onRemap();
}

off_t BlockStorage::_fileSize() {
	struct stat s;

	if(fstat(this->fd, &s) < 0) {
		perror("fstat");
		exit(errno);
	}

	return s.st_size;
}
