#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <inttypes.h>
#include <openssl/md5.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "btree.hpp"
#include "misc.hpp"

struct options {
	size_t blockSize;
	char forceCreateNewDb;
	char urlMode;
	char preSort;
	size_t preSortBufferSize;
};
/* ------------------------------------- */

static struct options OPTS;

void usage();
const char *getHost(const char *url);
const unsigned char *getHash(const char *string);

int main(int argc, char *argv[]) {
	const char *filename = "";
	unsigned long blockSize;
	unsigned long preSortBufferSize;

// 	size_t preSortBufferCurrentSize = 0;

	char ch;

	OPTS.blockSize = 4096*2;
	OPTS.forceCreateNewDb = 0;
	OPTS.urlMode = 0;
	OPTS.preSort = 0;
	OPTS.preSortBufferSize = 256;

	while ((ch = getopt(argc, argv, "scub:t:S:")) != -1) {
		switch (ch) {
			case 'b':
				blockSize = strtoul(optarg, NULL, 0);
				if(blockSize < 32 || blockSize == ULONG_MAX) {
					fputs("Block size must be >32\n", stderr);
					exit(255);
				}
				OPTS.blockSize = blockSize;
			break;
			case 't':
				filename = optarg;
			break;
			case 'c':
				OPTS.forceCreateNewDb = 1;
			break;
			case 's':
				OPTS.preSort = 1;
			break;
			case 'S':
				preSortBufferSize = strtoul(optarg, NULL, 0);
				if(preSortBufferSize < 32 || preSortBufferSize == ULONG_MAX) {
					fputs("Block size must be >32\n", stderr);
					exit(255);
				}
				OPTS.preSortBufferSize = preSortBufferSize;
			break;
			case 'u':
				OPTS.urlMode = 1;
			break;
			case '?':
			default:
				usage();
				exit(255);
		}
	}

// 	struct rlimit rl;
// 	if(getrlimit(RLIMIT_MEMLOCK, &rl) == 0) {
// 		if(rl.rlim_cur == RLIM_INFINITY)
// 			exit(0);
// 		printf("RLIMIT_MEMLOCK: %llu; max: %llu\n", (unsigned long long)rl.rlim_cur, (unsigned long long)rl.rlim_max);
// 	}

	if(!strlen(filename)) {
		usage();
		exit(255);
	}

	UniqueBTree tree(filename);

	if(access(filename, R_OK | W_OK) == 0 && !OPTS.forceCreateNewDb) {
		tree.load();
		fprintf(stderr, "Btree from %s with blockSize=%u was loaded\n", filename, tree.blockSize);
	} else {
		tree.create(OPTS.blockSize);
		fprintf(stderr, "New btree in %s with blockSize=%u was created\n", filename, tree.blockSize);
	}
/*
	for(unsigned int i=0; i<1400; i++) {
		tree.add(MD5((unsigned char *)&i, sizeof(i), NULL));
	}*/
	setlinebuf(stdin);
	setlinebuf(stdout);

	if(OPTS.preSort) {
		char *preSortBuffer;
		size_t preSortBufferCurrentSize = 0;
		size_t i;
		size_t itemSize = 8 + sizeof(void *);

		if(!(preSortBuffer = (char *)calloc(OPTS.preSortBufferSize, itemSize))) {
			perror("calloc");
			exit(errno);
		}

		while(1) {
			char newItem[8 + sizeof(void *)];
			char line[1024];
			const unsigned char *hash;
			off_t index;

			if(preSortBufferCurrentSize >= OPTS.preSortBufferSize) {
				char *popedItem = preSortBuffer + (preSortBufferCurrentSize - 1) * itemSize;
				char *lineS;
				char **ptr;

				ptr = (char **)(popedItem + 8);
				lineS = *ptr;

				if(tree.add(popedItem))
					fputs(lineS, stdout);

				free(lineS);
				preSortBufferCurrentSize--;
			}

			if(!fgets(line, sizeof(line), stdin))
				break;

			hash = getHash(line);

			memcpy(newItem, hash, 8);

			if(preSortBufferCurrentSize) {
				index = insertInSortedArray(preSortBuffer, itemSize, preSortBufferCurrentSize, newItem);
			} else {
				memcpy(preSortBuffer, newItem, itemSize);
				index = 0;
			}
			if(index != -1) {
				char *t = preSortBuffer + index * itemSize;
				char *lineS = (char *)malloc(strlen(line) + 1);
				char **ptr = (char **)(t + 8);
				strcpy(lineS, line);
				*ptr = lineS;

				preSortBufferCurrentSize++;
			}
		}

		for(i=0; i<preSortBufferCurrentSize; i++) {
			char *t = preSortBuffer + i * itemSize;
			char **ptr = (char **)(t + 8);
			char *lineS = *ptr;

			if(tree.add(t))
				fputs(lineS, stdout);
			free(lineS);
		}
		preSortBufferCurrentSize = 0;

	} else {
		char line[1024];
		while(fgets(line, sizeof(line), stdin)) {
			if(tree.add(getHash(line)))
				fputs(line, stdout);
		}
	}

	return EXIT_SUCCESS;
}

const char *getHost(const char *url) {
	static char host[128];
	size_t hostLen = 0;
	int numSlashes = 0;
	const char *chr;

	for(chr=url; *chr; chr++) {
		if(numSlashes == 2) {
			if(*chr == '/')
				break;
			host[hostLen] = *chr;
			hostLen++;
			if(hostLen >= sizeof(host) - 1)
				break;
		}

		if(*chr == '/')
			numSlashes++;
	}
	host[hostLen] = 0;
	return host;
}

void usage() {
	fputs("Usage: uniq [-uc] [-b blockSize] -t btreeFile\n", stderr);
}

const unsigned char *getHash(const char *string) {
	static unsigned char hashBuf[32];

	if(OPTS.urlMode) {
		const char *host = getHost(string);
		MD5((const unsigned char *)host, strlen(host), hashBuf);
		MD5((const unsigned char *)string, strlen(string), hashBuf+3);
	} else {
		MD5((const unsigned char *)string, strlen(string), hashBuf);
	}
	return hashBuf;
}

/* THE END */
