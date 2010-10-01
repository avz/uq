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
protected:
	int fd;
	void *map;
	size_t mapSize;

	char *filename;

	void _extendFile(size_t newSize);
	off_t _fileSize();

	bool needRemap;

public:
	BlockStorageSuperblock *superblock;
	BlockStorage(const char *filename);
	~BlockStorage();

	void create(size_t blockSize);
	void load();
	void flush();
	void mmap(size_t mapSize);
	void unmap();
	void remap();

	virtual void onRemap() = 0;

	Block allocate();
	Block get(uint32_t id);
};
#endif
