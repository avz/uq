#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "blockStorage/Storage.hpp"
#include "blockStorage/backend/File.hpp"
#include "ErrnoException.hpp"
#include "blockStorage/Superblock.hpp"

#include "external/murmurhash3/MurmurHash3.h"
#include "TokenReader.hpp"

class Block: public blockStorage::Block {
public:
	Block(uint64_t id, void *buf, ssize_t size):
		blockStorage::Block(id, buf, size)
	{
	}
};

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

template <typename StorageBackendT>
class Hash {
	blockStorage::Storage<StorageBackendT, Block, Superblock> &storage;

	bool _add(uint64_t blockId, uint64_t key, int level) {
		ssize_t offset = *((uint16_t *)&key + level);

		Block *b = this->storage.get(blockId);

		uint64_t *kb = (uint64_t *)b->buf;
//		fprintf(stderr, "OFFSET: %llu\n", (unsigned long long)offset);

		if(kb[offset]) {
			// либо такой айтем уже есть, либо надо идти вглубь
			if(kb[offset] & 0x8000000000000000ull) {
				// установлен старший бит - в ячейке хранится адрес следующего блока в дереве
				return this->_add(
					kb[offset] & 0x7fffffffffffffffull,
					key,
					level + 1
				);
			} else {
				// в дереве уже есть похожий айтем, надо проверить на полное соответствие
				if(kb[offset] == (key & 0x7fffffffffffffffull)) {
					// такой айтем уже есть
					return false;
				} else {
					// коллизия, нужно создавать новый блок дерева
					uint64_t oldKey = kb[offset];

					Block *nb = this->storage.allocate();

					// записываем адресок нового блока
					kb[offset] = nb->id | 0x8000000000000000ull;
					b->markAsDirty();

					// входим уже в новый блок, рекурсивно, на случай дальнейшего совпадения по ходу ключа

					this->_add(nb->id, oldKey, level + 1);
					return this->_add(nb->id, key, level + 1);
				}
			}

		} else {
			// такого айтема нет, добавляем
			// сброщенный старший бит указывает, что это айтем, а не ид следующего блока
			kb[offset] = key & 0x7fffffffffffffffull;
			b->markAsDirty();

			return true;
		}
	}
public:
	Hash(blockStorage::Storage<StorageBackendT, Block, Superblock> &storage):
		storage(storage)
	{
	};

	bool addKey(uint64_t key) {
		key &= 0x7fffffffffffffffull;

		return this->_add(this->storage.getSuperblock()->getFirstBlockId(), key, 0);
	};

	bool add(void *buf, ssize_t size) {
		uint64_t b[2];

		MurmurHash3_x64_128(buf, size, 0, &b);

		return this->addKey(b[0] ^ b[1]);
	}
};

int main(int argc, const char *argv[]) {
	try {
		int flags = O_RDWR | O_CREAT | O_TRUNC;
		blockStorage::backend::File backend("/tmp/test", flags);

		blockStorage::Storage<blockStorage::backend::File, Block, Superblock> storage(
			backend,
			8 * 256 * 256,
			flags
		);

		Superblock *sb = storage.getSuperblock();
		if(!sb->getFirstBlockId()) {
			sb->setFirstBlockId(storage.allocate()->id);
		}

		Hash<blockStorage::backend::File> hash(storage);

		void *buf;
		ssize_t bufLen;
		TokenReader reader(STDIN_FILENO);
		while((bufLen = reader.readUpToDelimiter('\n', &buf)) != 0) {
			if(bufLen < 0)
				throw ErrnoException(std::string("reading line from stdin"));

			hash.add(buf, bufLen);
//				fwrite(buf, bufLen, 1, stdout);
		}

//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0100000000000001ull));
//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0200000000000001ull));
//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0200000000000001ull));
//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0000000000010101ull));

	} catch(const ErrnoException &e) {
		fprintf(stderr, "ERRNOEXCEPTION!!!\n");
		e.print(stderr);
		exit(e.errorCode);
	} catch(const Exception &e) {
		fprintf(stderr, "EXCEPTION!!!\n");
		e.print(stderr);
		exit(254);
	}

	return EXIT_SUCCESS;
}
