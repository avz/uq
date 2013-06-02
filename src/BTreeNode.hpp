#ifndef BTREENODE_HPP
#define	BTREENODE_HPP

#include <stdlib.h>
#include <set>
#include "blockStorage/Block.hpp"
#include "BTree.hpp"

class BTree;

class BTreeNode: public blockStorage::Block {
	static const uint32_t NODE_ISLEAF = 1;

	uint32_t &flags;

	/**
	 * Для листовых нод: кол-во хешей в листе
	 * Для узловых: кол-во дочерних нод
	 */
	uint32_t &itemsCount;

	/**
	 * Только для листовых нод
	 * Указатель на начало блока хешей
	 */
	uint64_t *items;

	/**
	 * Для листовых нод: максимальное кол-во хешей в ноде
	 * Для узловых: максимальное суммарное кол-во идентификаторов дочерних нод и граничных ключей
	 */
	uint32_t maxItemsCount;

	/**
	 * Только для узловых нод.
	 * Максимальное кол-во дочерних нод
	 */
	uint32_t maxChildsCount;

	/**
	 * Только для узловых нод.
	 * Указатель на начало блока адресов чайлдов
	 */
	uint64_t *childs;

	/**
	 * Только для узловых нод.
	 * Указатель на начало блока граничных ключей
	 */
	uint64_t *childsSplitKeys;

	/**
	 * Сколько инсертов в ноду было сделано с момента выборки блока из хранилища
	 * или с момента последнего флаша
     */
	uint32_t numInserts;

	/**
	 * Нода была сконвертирована в std::set для скорости
	 */
	bool convertedToSet;

	/**
	 * Нода была сконвертирована в hash для скорости
	 */
	bool convertedToHash;

	std::set<uint64_t> set;

	void *hash;

public:
	BTreeNode(uint64_t id, void *buf, ssize_t size);

	void makeLeaf();

	/**
	 * Создать узловую ноду и назначить 2-х чайлдов
     * @param splitKey
     * @param leftNodeId
     * @param rightNodeId
     */
	void makeNode(uint64_t splitKey, uint64_t leftNodeId, uint64_t rightNodeId);

	/**
	 * Создать пустую узловую ноду
     */
	void makeNode();

	/**
	 *
     * @param key
	 * @param tree
	 * @param splitKey ключ, по которому засплитилось
     * @return 0 - если такой айтем есть, 1 - если вставлено и id новой ноды,
	 * если произошёл сплит (а значит, айтем добавлен)
     */
	uint64_t add(uint64_t key, BTree &tree, uint64_t *splitKey);

	void dump();
	void flush();

private:
	/**
     * @param items
     * @param count
     * @param newItem
     * @return NULL - если такой айтем уже есть
     */
	static uint64_t *_insert(uint64_t *items, uint32_t count, uint64_t newItem);

	/**
     * @param items
     * @param count
     * @param newItem
     * @return NULL - если такой айтем уже есть
     */
	static void _insertInto(uint64_t *items, uint32_t count, uint64_t newItem, uint64_t *position);

	/**
	 * Ищет место для вставки нового элемента, и возвращает указатель
	 * на элемент, на место которого надо вставить новый элемент. Или
	 * NULL, если такой элемент уже есть
     * @param items
     * @param count
     * @param newItem
     * @return NULL - если такой айтем уже есть
     */
	static uint64_t *_find(uint64_t *items, uint32_t count, uint64_t newItem);

	void _convertToSet();
	void _flushSet();
};

#endif	/* BTREENODE_HPP */
