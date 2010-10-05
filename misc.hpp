#ifndef MISC_H
#define MISC_H

#include <sys/types.h>
#include <stdint.h>
#include <string.h>

off_t searchInterval(void *arr, size_t itemSize, size_t itemsCount, const void *needle);
void insertInArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem, off_t place);
off_t insertInSortedArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem);
void *arrayPop(void *arr, size_t itemSize, size_t itemsCount, void *dst);

#endif
