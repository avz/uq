#ifndef STORAGE_HPP
#define STORAGE_HPP
#include <stdint.h>
#include <stdlib.h>
#include <map>

class Block {
public:
	bool needUpdate;
	int refCount;
	uint32_t id;
	char *ptr;

	Block(size_t blockSize);
	~Block();

	void free();
	void update();
};

class BlockStorageSuperblock {
public:
	Block *block;
	uint32_t &blockSize;
	uint32_t &blocksCount;

	BlockStorageSuperblock(Block *block);
	~BlockStorageSuperblock();
	void update();
};

class BlockStorage {
protected:
	int fd;
	uint32_t blockSize;
	char *filename;

	void _extendFile(off_t newSize);
	off_t _fileSize();
	void _gc();

	std::map<uint32_t, Block *> blocksCache;
public:
	BlockStorageSuperblock *superblock;
	BlockStorage(const char *filename);
	~BlockStorage();

	void create(size_t blockSize);
	void load();

	Block *allocate();
	Block *get(uint32_t id);
};
#endif
