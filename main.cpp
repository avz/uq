#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
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
#include "storage.hpp"

struct options {
	size_t blockSize;
	char forceCreateNewDb;
	char urlMode;
	char preSort;
	char ignoreCase;
	size_t preSortBufferSize;
	size_t cacheSize;
	size_t prefetchSize;

	// fields control
	int  fields_enabled;
	int  choose_field;
	char field_sep;
};
/* ------------------------------------- */

static struct options OPTS;

void usage();
const char *getHost(const char *url, size_t len);
const unsigned char *getHash(const char *string, int string_len);
int getStdinLine(char *buf, int buf_size, char **line_start, int *line_len);
void onSignal(int sig);

int main(int argc, char *argv[]) {
	const char *filename = "";
	unsigned long blockSize;
	unsigned long preSortBufferSize;
	unsigned long choose_field;
	unsigned long cacheSize;
	unsigned long prefetchSize;

	char ch;

	OPTS.blockSize = 4096*2;
	OPTS.forceCreateNewDb = 0;
	OPTS.urlMode = 0;
	OPTS.preSort = 0;
	OPTS.ignoreCase = 0;
	OPTS.preSortBufferSize = 256;
	OPTS.cacheSize = SIZE_T_MAX;

	// fields control
	OPTS.fields_enabled = 0;
	OPTS.choose_field   = 0;
	OPTS.field_sep      = ';';

	while ((ch = getopt(argc, argv, "sicub:t:S:f:d:m:p:")) != -1) {
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
			case 'i':
				OPTS.ignoreCase = 1;
			break;
			case 'S':
				preSortBufferSize = strtoul(optarg, NULL, 0);
				if(preSortBufferSize < 2 || preSortBufferSize == ULONG_MAX) {
					fputs("Pre-sort fuffer size must be >2\n", stderr);
					exit(255);
				}
				OPTS.preSortBufferSize = preSortBufferSize;
			break;
			case 'u':
				OPTS.urlMode = 1;
			break;
			case 'f':
				OPTS.fields_enabled = 1;
				choose_field = strtoul(optarg, NULL, 0);

				if(choose_field == ULONG_MAX || choose_field > INT_MAX) {
					fputs("Field must be int\n", stderr);
					exit(255);
				}
				OPTS.choose_field = choose_field;
			break;
			case 'm':
				cacheSize = strtoul(optarg, NULL, 0);

				if(cacheSize == ULONG_MAX || cacheSize > SIZE_T_MAX || cacheSize == 0) {
					fputs("Cache size must be positive\n", stderr);
					exit(255);
				}
				OPTS.cacheSize = (size_t)cacheSize;
			break;
			case 'p':
				prefetchSize = strtoul(optarg, NULL, 0);

				if(prefetchSize == ULONG_MAX || prefetchSize > SIZE_T_MAX || prefetchSize == 0) {
					fputs("Prefetch size must be positive\n", stderr);
					exit(255);
				}
				OPTS.prefetchSize = (size_t)prefetchSize;
			break;
			case 'd':
				if(strlen(optarg) != 1) {
					fputs("Field separator must be char\n", stderr);
					exit(255);
				}
				OPTS.field_sep      = *(char *)optarg;
			break;
			case '?':
			default:
				usage();
				exit(255);
		}
	}

	if(!strlen(filename)) {
		usage();
		exit(255);
	}

	signal(SIGHUP, onSignal);
	signal(SIGINT, onSignal);
	signal(SIGKILL, onSignal);
	signal(SIGPIPE, onSignal);
	signal(SIGTERM, onSignal);

	UniqueBTree tree(filename);
	tree.setCacheSize(OPTS.cacheSize);
	tree.setPrefetchSize(OPTS.prefetchSize);

	if(access(filename, R_OK | W_OK) == 0 && !OPTS.forceCreateNewDb) {
		tree.load();
		fprintf(stderr, "Btree from %s with blockSize=%u was loaded\n", filename, tree.blockSize);
	} else {
		tree.create(OPTS.blockSize);
		fprintf(stderr, "New btree in %s with blockSize=%u was created\n", filename, tree.blockSize);
	}

	setlinebuf(stdin);
	setlinebuf(stdout);

	char line[1024];
	char *line_ptr;
	int   line_len;

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

			if(getStdinLine(line, sizeof(line), &line_ptr, &line_len) == 0)
				break;

			hash = getHash(line_ptr, line_len);

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
		while(getStdinLine(line, sizeof(line), &line_ptr, &line_len)) {
			if(tree.add(getHash(line_ptr, line_len)))
				fputs(line, stdout);
		}
	}

	return EXIT_SUCCESS;
}

void onSignal(int sig) {
	fclose(stdin);
}

// returns 0 on EOF, 1 on success
int getStdinLine(char *buf, int buf_size, char **line_start, int *line_len){
	int eol, curr_field;
	char *curr, *next;

	do{
		if(!fgets(buf, buf_size, stdin))
			return 0;

		if(OPTS.fields_enabled == 0){
			*line_start = buf;
			*line_len   = strlen(buf);
			return 1;
		}

		curr = buf;
		eol  = 0;
		curr_field = 1;
		do{
			next = strchr(curr, OPTS.field_sep);
			if(next == NULL){
				if(curr_field == OPTS.choose_field){
					*line_start = curr;
					*line_len   = strlen(curr) - 1;
					return 1;
				}
				break;
			}

			if(curr_field == OPTS.choose_field){
				*line_start = curr;
				*line_len   = next - curr;
				return 1;
			}
			curr = next + 1; // skip field sep
			curr_field++;
		}while(curr);
	}while(1);
}

const char *getHost(const char *url, size_t len) {
	static char host[128];
	size_t hostLen = 0;
	int numSlashes = 0;
	size_t i;

	for(i=0; i < len && url[i]; i++) {
		if(numSlashes == 2) {
			if(url[i] == '/')
				break;
			host[hostLen] = url[i];
			hostLen++;
			if(hostLen >= sizeof(host) - 1)
				break;
		}

		if(url[i] == '/')
			numSlashes++;
	}

	host[hostLen] = 0;
	return host;
}

void usage() {
	fputs("Usage: uniq [-uc] [-S bufSize] [-b blockSize] -t btreeFile\n", stderr);
	fputs("\n", stderr);
	fputs("  -u        url mode\n", stderr);
	fputs("  -s        pre-sort input\n", stderr);
	fputs("  -S        pre-sort buffer size\n", stderr);
	fputs("  -f        select field\n", stderr);
	fputs("  -d        use given delimiter instead of ';'\n", stderr);
	fputs("  -i        ignore case\n", stderr);
}

const unsigned char *getHash(const char *string, int string_len) {
	static unsigned char hashBuf[32];
	const char *str;

	if(OPTS.ignoreCase) {
		str = (char *)alloca(string_len);
		strtolower((char *)str, string, string_len);
	} else {
		str = string;
	}


	if(OPTS.urlMode) {
		const char *host = getHost(str, string_len);
		MD5((const unsigned char *)host, strlen(host), hashBuf);
		MD5((const unsigned char *)str, string_len, hashBuf+3);
	} else {
		MD5((const unsigned char *)str, string_len, hashBuf);
	}
	return hashBuf;
}

/* THE END */
