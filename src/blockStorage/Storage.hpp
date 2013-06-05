#ifndef BLOCKSTORAGE_STORAGE_HPP
#define	BLOCKSTORAGE_STORAGE_HPP
#include <sys/types.h>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <vector>
#include <assert.h>
#include <fcntl.h>

#include "Block.hpp"

class _BlockIdToHash {
public:
    std::size_t operator()(uint64_t k) const {
        return (std::size_t)k;
    }
};

namespace blockStorage {
	template <typename BackendT, typename BlockT, typename SuperblockT>
	class Storage {
		std::unordered_map<uint64_t, BlockT *, _BlockIdToHash> cache;

		BackendT &backend;
		ssize_t superblockSize;
		int flags;

		BlockT *create(uint64_t id, void *buf, ssize_t size);
		void addToCache(BlockT *block);
		BlockT *getFromCache(uint64_t id);

		void create();

		SuperblockT *superblock;

	public:
		Storage(BackendT &backend, ssize_t superblockSize, int flags);
		~Storage();

		BlockT *get(uint64_t id, ssize_t size);
		SuperblockT *getSuperblock();
		BlockT *allocate(ssize_t size);

		void flushBlock(BlockT *block);
		void flush();
		void clearCache();
	};
}

/* template implementation */
template<typename BackendT, typename BlockT, typename SuperblockT>
blockStorage::Storage<BackendT, BlockT, SuperblockT>::Storage(BackendT& backend, ssize_t superblockSize, int flags):
	backend(backend),
	superblockSize(superblockSize),
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

	off_t off = this->backend.allocate(this->superblockSize);
	assert(off == 0);

	this->superblock = new SuperblockT(
		this->backend.read(0, this->superblockSize),
		this->superblockSize
	);

	this->superblock->create();
}

template<typename BackendT, typename BlockT, typename SuperblockT>
SuperblockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::getSuperblock() {
	if(!this->superblock) {
		this->superblock = new SuperblockT(
			this->backend.read(0, this->superblockSize),
			this->superblockSize
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
	typename std::unordered_map<uint64_t, BlockT *, _BlockIdToHash>::iterator i;

	i = this->cache.find(id);

	if(i == this->cache.end())
		return NULL;

	return i->second;
}

template<typename BackendT, typename BlockT, typename SuperblockT>
BlockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::get(uint64_t id, ssize_t size) {
	assert(id != 0);

	BlockT *b = this->getFromCache(id);

	if(!b) {
		void *buf = this->backend.read((off_t)id, size);

		b = this->create(id, buf, size);
	}

	return b;
}

template<typename BackendT, typename BlockT, typename SuperblockT>
BlockT *blockStorage::Storage<BackendT, BlockT, SuperblockT>::allocate(ssize_t size) {
	off_t off = this->backend.allocate(size);

	return this->create(
		(uint64_t)off,
		this->backend.read(off, size),
		size
	);
}

template<typename BackendT, typename BlockT, typename SuperblockT>
void blockStorage::Storage<BackendT, BlockT, SuperblockT>::flush() {
	typename std::unordered_map<uint64_t, BlockT *, _BlockIdToHash>::iterator i;

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

	this->backend.write(block->buf, (off_t)block->id, block->size);

	block->markAsClean();
}

template<typename BackendT, typename BlockT, typename SuperblockT>
void blockStorage::Storage<BackendT, BlockT, SuperblockT>::clearCache() {
	typename std::unordered_map<uint64_t, BlockT *, _BlockIdToHash>::iterator i;

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

