#ifndef STORAGE_HPP
#define STORAGE_HPP
#include <stdint.h>
#include <stdlib.h>

class Block {
public:
	uint32_t id;
	void *ptr;

// 	Block();
// 	~Block();
};

class BlockStorageSuperblock {
public:
	uint32_t &blockSize;
	uint32_t &blocksCount;

	BlockStorageSuperblock(void *ptr);
};

class BlockStorage {
	int fd;
	void *map;
	size_t mmapedLen;

	char *filename;

	void _doMmap();
	void _extend(size_t newSize);
public:
	BlockStorageSuperblock *superblock;
	BlockStorage(const char *filename);
	~BlockStorage();

	void create(size_t blockSize);
	void load();
	void flush();
	void remap();
	Block allocate();
	Block get(uint32_t id);
};
#endif
