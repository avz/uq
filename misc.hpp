#ifndef MISC_H
#define MISC_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define fatal_input(errorString) { fprintf(stderr, "%s\n", errorString); exit(255); }
#define fatal_code(errorString, code) { fprintf(stderr, "%s: %s [%s:%u]\n", errorString, strerror(code), __FILE__, __LINE__); exit(code); }
#define fatal(errorString) { fprintf(stderr, "%s: %s [%s:%u]\n", errorString, strerror(errno), __FILE__, __LINE__); exit(errno); }

off_t searchInterval(void *arr, size_t itemSize, size_t itemsCount, const void *needle);
void insertInArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem, off_t place);
off_t insertInSortedArray(void *arr, size_t itemSize, size_t itemsCount, const void *newItem);
char *strtolower(char *dst, const char *src, size_t len);
double gettimed();
void printDump(FILE *fd, const char *buf, size_t bufLen);

#endif
