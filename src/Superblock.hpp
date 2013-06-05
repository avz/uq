#ifndef SUPERBLOCK_HPP
#define	SUPERBLOCK_HPP

#include "blockStorage/Superblock.hpp"

class Superblock: public blockStorage::Superblock {
	void *head;
	uint64_t firstBlockId;
	uint32_t rootLevel;

public:
	Superblock(void *buf, ssize_t size):
		blockStorage::Superblock(buf, size)
	{
	};

	void create() {
		blockStorage::Superblock::create();

		this->head = (char *)this->buf + this->headerSize;
		this->firstBlockId = 0;
		this->rootLevel = 0;
	};

	void flush() {
		blockStorage::Superblock::flush();

		*((uint32_t *)this->head) = 0xffffffff;
		*((uint64_t *)((uint32_t *)this->head + 1)) = this->firstBlockId;
		*((uint32_t *)((uint32_t *)this->head + 3)) = this->rootLevel;

		fprintf(stderr, "flush()\n");
	};

	void read() {
		blockStorage::Superblock::read();

		this->head = (char *)this->buf + this->headerSize;

		if(*((uint32_t *)this->head) != 0xffffffff)
			throw Exception(std::string("Invalid header"));

		this->firstBlockId = *((uint64_t *)((uint32_t *)this->head + 1));
		this->rootLevel = *((uint32_t *)((uint32_t *)this->head + 3));
	};

	void setFirstBlock(uint64_t id, uint32_t level) {
		this->firstBlockId = id;
		this->rootLevel = level;
		fprintf(stderr, "First block ID: %llu\n", (unsigned long long)id);

		this->markAsDirty();
	}

	uint64_t getFirstBlockId() {
		return this->firstBlockId;
	}

	uint32_t getRootLevel() {
		return this->rootLevel;
	}
};

#endif	/* SUPERBLOCK_HPP */

