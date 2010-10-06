#ifndef STORAGE_HPP
#define STORAGE_HPP
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <map>
#include <list>

#ifndef SIZE_T_MAX
	#define SIZE_T_MAX (~((size_t)0))
#endif

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

	size_t gcTimer;
	size_t gcProbability;
	void _gc(bool cleanAll = false);

	std::map<uint32_t, std::list<Block *>::iterator> blocksCacheMap;
	std::list<Block *> blocksCache;
	size_t blocksCacheCurrentSize;

	size_t blocksCacheSize;
	size_t prefetchSize;
public:
	BlockStorageSuperblock *superblock;
	BlockStorage(const char *filename);
	~BlockStorage();

	void create(size_t blockSize);
	void load();
	void setCacheSize(size_t size);
	void setPrefetchSize(size_t size);

	Block *allocate();
	Block *get(uint32_t id);
};
#endif
