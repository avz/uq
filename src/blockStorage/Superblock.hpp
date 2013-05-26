#ifndef BLOCKSTORAGE_SUPERBLOCK_HPP
#define	BLOCKSTORAGE_SUPERBLOCK_HPP

#include <sys/types.h>
#include <string>
#include "Block.hpp"
#include "../Exception.hpp"

namespace blockStorage {
	class Superblock: public Block {
	public:
		uint32_t headerSize;
		uint32_t blockSize;

		Superblock(void *buf, ssize_t size):
			Block(0, buf, size),
			headerSize(0),
			blockSize(size)
		{
			fprintf(stderr, "Superblock()\n");
		};

		~Superblock() {
			fprintf(stderr, "~Superblock()\n");
		};

		void create() {
			char *b = (char *)this->buf;

			b[0] = 'U';
			b[1] = 'Q';
			b[2] = 'T';
			b[3] = 0;

			this->headerSize = 4 + 4 + 4;

			this->markAsDirty();
		}

		void flush() {
			*((uint32_t *)this->buf + 1) = 4 + 4 + 4;
			*((uint32_t *)this->buf + 2) = this->blockSize;
		}

		void read() {
			char *b = (char *)this->buf;

			if(b[0] != 'U' || b[1] != 'Q' || b[2] != 'T' || b[3] != 0)
				throw Exception(std::string("Invalid magic"));

			this->headerSize = *((uint32_t *)this->buf + 1);
			this->blockSize = *((uint32_t *)this->buf + 2);
		}
	};
}

#endif	/* BLOCKSTORAGE_SUPERBLOCK_HPP */

