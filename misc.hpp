#ifndef MISC_H
#define MISC_H

#include <sys/types.h>
#include <stdint.h>
#include <string.h>

off_t searchInterval(void *arr, size_t itemSize, size_t itemsCount, const void *needle);
void insertInArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem, off_t place);
off_t insertInSortedArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem);
char *strtolower(char *dst, const char *src, size_t len);
double gettimed();

#endif
