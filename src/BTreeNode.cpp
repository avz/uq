#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <algorithm>
#include "BTreeNode.hpp"

//class _BTreeNodeHashAllocator {
//public:

//};

BTreeNode::BTreeNode(uint64_t id, void *buf, ssize_t size):
	blockStorage::Block(id, buf, size),
	flags(*(uint32_t *)buf),
	itemsCount(*((uint32_t *)buf + 1)),
	items((uint64_t *)((uint32_t *)buf + 2)),
	maxItemsCount(size / sizeof(uint64_t) - 1),
	maxChildsCount(maxItemsCount - (maxItemsCount - 1) / 2),
	childs(items),
	childsSplitKeys(childs + maxChildsCount),
	numInserts(0),
	convertedToSet(0)
{
}

void BTreeNode::makeLeaf() {
	this->flags = 0;
	this->itemsCount = 0;
}

void BTreeNode::makeNode(uint64_t splitKey, uint64_t leftNodeId, uint64_t rightNodeId) {
	this->makeNode();

	this->itemsCount = 2;

	this->childs[0] = leftNodeId;
	this->childs[1] = rightNodeId;

	this->childsSplitKeys[0] = splitKey;
}

void BTreeNode::makeNode() {
	this->flags = 0;
	this->itemsCount = 0;
}

uint64_t BTreeNode::add(uint64_t key, BTree &tree, uint64_t *splitKey) {
//	fprintf(stderr, "add(): %lu\n", this->id);
//	this->dump();

	if(!this->level) { // is leaf
		if(this->convertedToSet) {
			if(this->set.insert(key).second) {
				if(this->set.size() + 2 > this->maxItemsCount)
					this->_flushSet();

				return 1;
			} else {
				return 0;
			}
		}

		if(BTreeNode::_insert(this->items, this->itemsCount, key)) {
			// айтем добавлен, проверяем нужно ли сделать сплит
			this->itemsCount++;
			this->numInserts++;
			this->markAsDirty();

//			if(this->numInserts > 5)
//				this->_convertToSet();

			if(this->itemsCount + 1 >= this->maxItemsCount) {
				// нужен сплит, запрашиваем новую ноду и заполняем её
//fprintf(stderr, "split: %lu\n", this->id);
//this->dump();
				BTreeNode *node = tree.allocateLeaf(this->level);
				uint32_t splitIndex = this->itemsCount / 2;
				*splitKey = this->items[splitIndex];

				// сгружаем в новую ноду всё что правее splitIndex, включая его самого
				memcpy(node->items, this->items + splitIndex, (this->itemsCount - splitIndex) * sizeof(*this->items));

				node->itemsCount = this->itemsCount - splitIndex;
				this->itemsCount = splitIndex;
//this->dump();
//node->dump();
				return node->id;
			}

			return 1;
		}

		return 0;
	} else {
		uint64_t *position = BTreeNode::_find(this->childsSplitKeys, this->itemsCount - 1, key);

		if(!position) {
			// Такой ключ является одним из сплит-ключей ноды, то есть уже существует
			return 0;
		}

		uint32_t childOffset = position - this->childsSplitKeys;
//		if(!this->childs[childOffset])
//			this->dump();

		BTreeNode *child;

		if(this->level > 1)
			child = tree.getNode(this->childs[childOffset], this->level - 1);
		else
			child = tree.getLeaf(this->childs[childOffset], this->level - 1);

		uint64_t inserted = child->add(key, tree, splitKey);
		if(!inserted) // ключ уже есть
			return 0;

		if(inserted == 1) // добавили
			return 1;

		/*
		 * Дочерняя нода сплитнулась, значит, надо её прописать,
		 * в самом худшем случае придётся сплитнуть и эту ноду.
		 * Позицию, в которую вставлять, мы уже знаем с предыдущего шага,
		 * сплит-кей пришёл в splitKey.
		 * Новая нода встаёт справа от существующей.
		 */
		BTreeNode::_insertInto(this->childsSplitKeys, this->itemsCount - 1, *splitKey, position);
		BTreeNode::_insertInto(this->childs, this->itemsCount, inserted, this->childs + childOffset + 1);

		this->itemsCount++;

		if(this->itemsCount + 1 >= this->maxChildsCount) {
			// ноду пора сплитить
			BTreeNode *node = tree.allocateNode(this->level);

			uint32_t splitIndex = (this->itemsCount - 1) / 2;
			*splitKey = this->childsSplitKeys[splitIndex];

			memcpy(
				node->childs,
				this->childs + splitIndex + 1,
				(this->itemsCount - splitIndex - 1) * sizeof(*this->childs)
			);

			memcpy(
				node->childsSplitKeys,
				this->childsSplitKeys + splitIndex + 1,
				(this->itemsCount - 1 - splitIndex - 1) * sizeof(*this->childsSplitKeys)
			);

			node->itemsCount = (this->itemsCount - splitIndex - 1);
			this->itemsCount -= node->itemsCount;

			return node->id;
		}

		this->markAsDirty();

		return 1;
	}
}

/**
 * @param items
 * @param count
 * @param newItem
 * @return NULL - если такой айтем уже есть
 */
inline uint64_t *BTreeNode::_insert(uint64_t *items, uint32_t count, uint64_t newItem) {
	uint64_t *position = BTreeNode::_find(items, count, newItem);

	if(!position)
		return NULL;

	BTreeNode::_insertInto(items, count, newItem, position);

	return position;
}

/**
 * @param items
 * @param count
 * @param newItem
 * @return NULL - если такой айтем уже есть
 */
inline void BTreeNode::_insertInto(uint64_t *items, uint32_t count, uint64_t newItem, uint64_t *position) {
	if(count && position != items + count)
		memmove(position + 1, position, (items + count - position) * sizeof(*items));

	*position = newItem;
}

inline static uint64_t *_findApprox(uint64_t *items, uint32_t count, uint64_t newItem) {
	if(!count || newItem < items[0]) {
		return items;
	} else if(count == 1) {
		if(newItem > items[0])
			return items + 1;
		else if(newItem < items[0])
			return items;
		else
			return NULL;
	}

	uint32_t approxOffset = (newItem - items[0]) / ((items[count - 1] - items[0]) / count);
	uint64_t approxItem = items[approxOffset];

	if(newItem == approxItem)
		return NULL;

	if(newItem > approxItem) {
		// идём вниз
		for(uint32_t i = approxOffset + 1; i < count; i++) {
			if(newItem == items[i])
				return NULL;

			if(newItem < items[i])
				return items + i;
		}

		return items + count;
	} else {
		// идём вверх
		for(uint32_t i = approxOffset - 1; i >= 0; i--) {
			if(newItem == items[i])
				return NULL;

			if(newItem > items[i])
				return items + i + 1;
		}

		return items;
	}
}

/**
 * Ищет место для вставки нового элемента, и возвращает указатель
 * на элемент, на место которого надо вставить новый элемент. Или
 * NULL, если такой элемент уже есть
 * @param items
 * @param count
 * @param newItem
 * @return NULL - если такой айтем уже есть
 */
inline uint64_t *BTreeNode::_find(uint64_t *items, uint32_t count, uint64_t newItem) {
	if(count < 2048)
		return _findApprox(items, count, newItem);

	uint32_t left = 0;
	uint32_t right = count - 1;

	if(newItem >= items[right]) {
		if(newItem == items[right])
			return NULL;

		return items + right + 1;
	}

	if(newItem <= items[left]) {
		if(newItem == items[left])
			return NULL;

		return items;
	}

	while(right - left > 2048) {
		uint32_t cindex = left + (right - left) / 2;
		uint64_t ci = items[cindex];

		if(newItem > ci) {
			left = cindex;
		} else if(newItem < ci) {
			right = cindex;
		} else { // ci == newItem
			return NULL;
		}
	}

	return _findApprox(items + left, right - left, newItem);
}

void BTreeNode::dump() {
//	/**
//	 * Для листовых нод: кол-во хешей в листе
//	 * Для узловых: кол-во дочерних нод
//	 */
//	uint32_t &itemsCount;
//
//	/**
//	 * Только для листовых нод
//	 * Указатель на начало блока хешей
//	 */
//	uint64_t *items;
//
//	/**
//	 * Для листовых нод: максимальное кол-во хешей в ноде
//	 * Для узловых: максимальное суммарное кол-во идентификаторов дочерних нод и граничных ключей
//	 */
//	uint32_t maxItemsCount;
//
//	/**
//	 * Только для узловых нод.
//	 * Максимальное кол-во дочерних нод
//	 */
//	uint32_t maxChildsCount;
//
//	/**
//	 * Только для узловых нод.
//	 * Указатель на начало блока адресов чайлдов
//	 */
//	uint64_t *childs;
//
//	/**
//	 * Только для узловых нод.
//	 * Указатель на начало блока граничных ключей
//	 */
//	uint64_t *childsSplitKeys;
	fprintf(stderr, "%s node #%" PRIu64 ":\n", (!this->level ? "Leaf" : "Index"), this->id);
	fprintf(stderr, "\tmaxItemsCount: %" PRIu32, this->maxItemsCount);
	fprintf(stderr, "\titemsCount: %" PRIu32, this->itemsCount);
	if(!this->level) {
		fprintf(stderr, "\titems: ");
		for(uint32_t i = 0; i < this->itemsCount; i++) {
			fprintf(stderr, "#%" PRIu64 " ", this->items[i]);
		}

		fprintf(stderr, "\n");
	} else {
		fprintf(stderr, "\titems: ");
		if(this->itemsCount) {

			for(uint32_t i = 0; i < this->itemsCount - 1; i++) {
				fprintf(stderr, "#%" PRIu64 " ", this->childs[i]);
				fprintf(stderr, "<%" PRIu64 "> ", this->childsSplitKeys[i]);
			}

			fprintf(stderr, "#%" PRIu64 " ", this->childs[this->itemsCount - 1]);
		}
		fprintf(stderr, "\n");
	}
}

void BTreeNode::flush() {
	if(this->convertedToSet)
		this->_flushSet();
}

void BTreeNode::_convertToSet() {
//	fprintf(stderr, "Convert to set #%" PRIu64 "\n", this->id);
	this->convertedToSet = true;

	this->set.reserve(this->maxItemsCount);

	for(uint32_t i = 0; i < this->itemsCount; i++) {
		this->set.insert(this->items[i]);
	}
}

void BTreeNode::_flushSet() {
//	fprintf(stderr, "Flush set #%" PRIu64 "\n", this->id);
	this->convertedToSet = false;
	this->numInserts = 0;

	std::vector<uint64_t> order;

	order.resize(this->set.size());

	std::copy(this->set.begin(), this->set.end(), order.begin());
	std::sort(order.begin(), order.end());

	std::vector<uint64_t>::iterator i;
	uint32_t n = 0;

	for(i = order.begin(); i != order.end(); ++i) {
		this->childs[n] = *i;
		n++;
	}

	this->set.clear();

	this->itemsCount = n;

	this->markAsDirty();
}
//-
