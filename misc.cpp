#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

off_t searchInterval(void *arr, size_t itemSize, size_t itemsCount, const void *needle) {
	off_t start = 0;
	off_t end = itemsCount - 1;

	off_t median;
	int t;

	t = memcmp(needle, arr, itemSize);
	if(t == 0)
		return -1;
	else if(t < 0)
		return 0;

	t = memcmp(needle, (char *)arr + (itemsCount - 1) * itemSize, itemSize);

	if(t == 0)
		return -1;
	else if(t > 0)
		return itemsCount;

	do {
		if(end - start <= 1)
			return start + 1;

		median = (end - start) / 2 + start;

		t = memcmp(needle, (char *)arr + median * itemSize, itemSize);
		if(t > 0)
			start = median;
		else if(t < 0)
			end = median;
		else
			return -1;
	} while(true);
}

void insertInArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem, off_t place) {
	size_t copySize = (itemsCount - place) * itemSize;
	char *t = (char *)arr + place * itemSize;
	memmove(t + itemSize, t, copySize);
	memcpy(t, newItem, itemSize);
}

off_t insertInSortedArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem) {
	off_t off = searchInterval(arr, itemSize, itemsCount, newItem);
	if(off < 0)
		return -1;

	insertInArray(arr, itemSize, itemsCount, newItem, off);
	return off;
}

char *strtolower(char *dst, const char *src, size_t len) {
	size_t i;
	for(i=0; i < len; i++)
		dst[i] = tolower(src[i]);

	return dst;
}
