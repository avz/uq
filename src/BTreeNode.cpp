#include "BTreeNode.hpp"

BTreeNode::BTreeNode(uint64_t id, void *buf, ssize_t size):
	blockStorage::Block(id, buf, size),
	flags(*(uint32_t *)buf),
	itemsCount(*((uint32_t *)buf + 1)),
	items((uint64_t *)((uint32_t *)buf + 2)),
	maxItemsCount(size / sizeof(uint64_t) - 1),
	maxChildsCount(maxItemsCount - (maxItemsCount - 1) / 2),
	childs(items),
	childsSplitKeys(childs + maxChildsCount)
{
}

void BTreeNode::makeLeaf() {
	this->flags = BTreeNode::NODE_ISLEAF;
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
	if(this->flags & BTreeNode::NODE_ISLEAF) {
		if(BTreeNode::_insert(this->items, this->itemsCount, key)) {
			// айтем добавлен, проверяем нужно ли сделать сплит
			this->itemsCount++;

			if(this->itemsCount + 1 >= this->maxItemsCount) {
				// нужен сплит, запрашиваем новую ноду и заполняем её

				BTreeNode *node = tree.allocate();
				node->makeLeaf();
				uint32_t splitIndex = this->itemsCount / 2;
				*splitKey = this->items[splitIndex];

				// сгружаем в новую ноду всё что правее splitIndex, включая его самого
				memcpy(node->items, this->items + splitIndex, (this->itemsCount - splitIndex) * sizeof(*this->items));

				node->itemsCount = this->itemsCount - splitIndex;
				this->itemsCount = splitIndex;

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

		BTreeNode *child = tree.get(this->childs[childOffset]);

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
			BTreeNode *node = tree.allocate();
			node->makeNode();

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

		return 1;
	}
}

/**
 * @param items
 * @param count
 * @param newItem
 * @return NULL - если такой айтем уже есть
 */
//	static uint64_t *_insertOpt(uint64_t *items, uint32_t count, uint64_t newItem) {
//		uint64_t left = 0;
//		uint64_t right = count - 1;
//
//		if(!count) {
//			items[0] = newItem;
//			return items;
//		}
//
//		if(newItem >= items[right]) {
//			if(newItem == items[right])
//				return NULL;
//
//			items[right + 1] = newItem;
//
//			return items + right + 1;
//		}
//
//		if(newItem <= items[left]) {
//			if(newItem == items[left])
//				return NULL;
//
//			memmove(items + 1, items, count * sizeof(*items));
//
//			items[0] = newItem;
//
//			return items;
//		}
//
//		while(right - left > 1) {
//			uint32_t cindex = left + (right - left) / 2;
//			uint64_t ci = items[cindex];
//
//			if(newItem > ci) {
//				left = cindex;
//			} else if(newItem < ci) {
//				right = cindex;
//			} else { // ci == newItem
//				return NULL;
//			}
//		}
//
//		// вставляем между left и right
//		memmove(items + right + 1, items + right, (count - right) * sizeof(*items));
//
//		items[right] = newItem;
//
//		return items + right;
//	}

uint64_t *BTreeNode::_insert(uint64_t *items, uint32_t count, uint64_t newItem) {
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
void BTreeNode::_insertInto(uint64_t *items, uint32_t count, uint64_t newItem, uint64_t *position) {
	if(count && position == items + count)
		memmove(position + 1, position, (items + count - position) * sizeof(*items));

	*position = newItem;
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
uint64_t *BTreeNode::_find(uint64_t *items, uint32_t count, uint64_t newItem) {
	uint64_t left = 0;
	uint64_t right = count - 1;

	if(!count)
		return items;

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

	while(right - left > 1) {
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

	return items + right;
}

//-
