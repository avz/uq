#ifndef BLOCKSTORAGE_BLOCK_HPP
#define	BLOCKSTORAGE_BLOCK_HPP

#include <sys/types.h>
#include <inttypes.h>

namespace blockStorage {
	class Block {
		/**
		 * Если true, то блок нужно сохранить в хранилище.
		 */
		bool dirty;

		/**
		 * Счётчик потребителей, желающих, чтобы этот блок не освобождался.
		 * Если > 0, то gc будет игнорировать этот блок
		 */
		int persistency;

	public:
		void *buf;
		ssize_t size;

		uint64_t id;

		/**
		 * @param id
         * @param buf
         * @param size
         */
		Block(uint64_t id, void *buf, ssize_t size):
			dirty(false),
			persistency(0),
			buf(buf),
			size(size),
			id(id)
		{};

		void markAsDirty() {
			this->dirty = true;
		};

		void markAsClean() {
			this->dirty = false;
		};

		void flush() {

		};

		void allocate() {

		};

		bool isDirty() {
			return this->dirty;
		};

		void incPersistency() {
			this->persistency++;
		};

		void decPersistency() {
			this->persistency--;
		};
	};
}

#endif	/* BLOCKSTORAGE_BLOCK_HPP */
