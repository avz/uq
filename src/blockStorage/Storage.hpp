#ifndef BLOCKSTORAGE_STORAGE_HPP
#define	BLOCKSTORAGE_STORAGE_HPP
#include <sys/types.h>
#include <stdint.h>
#include <map>
#include <vector>
#include <assert.h>
#include <fcntl.h>

#include "Block.hpp"

namespace blockStorage {
	template<typename BackendT, typename BlockT, typename SuperblockT>
	class Storage {
		std::map<uint64_t, BlockT *> cache;

		BackendT &backend;
		ssize_t blockSize;
		int flags;

		BlockT *create(uint64_t id, void *buf, ssize_t size);
		void addToCache(BlockT *block);
		BlockT *getFromCache(uint64_t id);

		void create();

		SuperblockT *superblock;

	public:
		Storage(BackendT &backend, ssize_t blockSize, int flags);
		~Storage();

		BlockT *get(uint64_t id);
		SuperblockT *getSuperblock();
		BlockT *allocate();

		void flushBlock(BlockT *block);
		void flush();
		void clearCache();
	};
}

/* template implementation */
template<typename BackendT, typename BlockT, typename SuperblockT>
blockStorage::Storage<BackendT, BlockT, SuperblockT>::Storage(BackendT& backend, ssize_t blockSize, int flags):
	backend(backend),
	blockSize(blockSize),
	flags(flags),
	superblock(NULL)
{
	if(this->flags & O_TRUNC)
		backend.clear();

	if(!this->backend.size())
		this->create();
}

template<typename BackendT, typename BlockT, typename SuperblockT>
blockStorage::Storage<BackendT, BlockT, SuperblockT>::~Storage() {
	this->flush();
	this->clearCache();
}

template<typename BackendT, typename BlockT, typename SuperblockT>
void blockStorage::Storage<BackendT, BlockT, SuperblockT>::create() {
	backend.clear();

	off_t off = this->backend.allocate(this->blockSize);
	assert(off == 0);

	this->superblock = new SuperblockT(
		this->backend.read(0, this->blockSize),
		this->blockSize
	);

	this->superblock->create();
}

template<typename BackendT, typename BlockT, typename SuperblockT>
SuperblockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::getSuperblock() {
	if(!this->superblock) {
		this->superblock = new SuperblockT(
			this->backend.read(0, this->blockSize),
			this->blockSize
		);

		this->superblock->read();
	}

	return this->superblock;
}

template<typename BackendT, typename BlockT, typename SuperblockT>
BlockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::create(uint64_t id, void *buf, ssize_t size) {
	BlockT *block = new BlockT(id, buf, size);

	this->addToCache(block);

	return block;
}

template<typename BackendT, typename BlockT, typename SuperblockT>
void blockStorage::Storage<BackendT, BlockT, SuperblockT>::addToCache(BlockT *block) {
	assert(
		this->cache.insert(std::pair<uint64_t, BlockT *>(block->id, block)).second
	);
}

template<typename BackendT, typename BlockT, typename SuperblockT>
BlockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::getFromCache(uint64_t id) {
	typename std::map<uint64_t, BlockT *>::iterator i;

	i = this->cache.find(id);

	if(i == this->cache.end())
		return NULL;

	return i->second;
}

template<typename BackendT, typename BlockT, typename SuperblockT>
BlockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::get(uint64_t id) {
	assert(id != 0);

	BlockT *b = this->getFromCache(id);

	if(!b) {
		void *buf = this->backend.read((off_t)id, this->blockSize);

		b = this->create(id, buf, this->blockSize);
	}

	return b;
}

template<typename BackendT, typename BlockT, typename SuperblockT>
BlockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::allocate() {
	off_t off = this->backend.allocate(this->blockSize);

	return this->create(
		(uint64_t)off,
		this->backend.read(off, this->blockSize),
		this->blockSize
	);
}

template<typename BackendT, typename BlockT, typename SuperblockT>
void blockStorage::Storage<BackendT, BlockT, SuperblockT>::flush() {
	typename std::map<uint64_t, BlockT *>::iterator i;

	for(i = this->cache.begin(); i != this->cache.end(); ++i) {
		BlockT *b = i->second;
		if(b->isDirty())
			this->flushBlock(b);
	}

	if(this->superblock && this->superblock->isDirty()) {
		this->superblock->flush();

		this->backend.write(this->superblock->buf, (off_t)this->superblock->id, this->superblock->size);

		this->superblock->markAsClean();
	}
}

template<typename BackendT, typename BlockT, typename SuperblockT>
void blockStorage::Storage<BackendT, BlockT, SuperblockT>::flushBlock(BlockT *block) {
	block->flush();

	this->backend.write(block->buf, (off_t)block->id, this->blockSize);

	block->markAsClean();
}

template<typename BackendT, typename BlockT, typename SuperblockT>
void blockStorage::Storage<BackendT, BlockT, SuperblockT>::clearCache() {
	typename std::map<uint64_t, BlockT *>::iterator i;

	for(i = this->cache.begin(); i != this->cache.end(); ++i) {
		this->backend.free(i->second->buf);
		delete i->second;
	}

	this->cache.clear();

	if(this->superblock) {
		this->backend.free(this->superblock->buf);
		delete this->superblock;
		this->superblock = NULL;
	}
}

#endif	/* BLOCKSTORAGE_STORAGE_HPP */

