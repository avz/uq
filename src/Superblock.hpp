#ifndef SUPERBLOCK_HPP
#define	SUPERBLOCK_HPP

#include "blockStorage/Superblock.hpp"

class Superblock: public blockStorage::Superblock {
	void *head;
	uint64_t firstBlockId;

public:
	Superblock(void *buf, ssize_t size):
		blockStorage::Superblock(buf, size)
	{
	};

	void create() {
		blockStorage::Superblock::create();

		this->head = (char *)this->buf + this->headerSize;
		this->firstBlockId = 0;
	};

	void flush() {
		blockStorage::Superblock::flush();

		*((uint32_t *)this->head) = 1;
		*((uint64_t *)((uint32_t *)this->head + 1)) = this->firstBlockId;

		fprintf(stderr, "flush()\n");
	};

	void read() {
		blockStorage::Superblock::read();

		this->head = (char *)this->buf + this->headerSize;

		if(*((uint32_t *)this->head) != 1)
			throw Exception(std::string("Invalid header"));

		this->firstBlockId = *((uint64_t *)((uint32_t *)this->head + 1));
	};

	void setFirstBlockId(uint64_t id) {
		this->firstBlockId = id;
		fprintf(stderr, "First block ID: %llu\n", (unsigned long long)id);

		this->markAsDirty();
	}

	uint64_t getFirstBlockId() {
		return this->firstBlockId;
	}
};

#endif	/* SUPERBLOCK_HPP */

