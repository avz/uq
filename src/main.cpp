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

#include <map>
#include <string>

#include "btree.hpp"
#include "misc.hpp"
#include "storage.hpp"
#include "token_reader.hpp"

struct options {
	size_t blockSize;
	char forceCreateNewDb;
	char urlMode;
	char verbose;
	size_t cacheSize;
	size_t prefetchSize;

	// fields control
	int keyField;
	int keyFieldSeparator;

	TokenReader *reader;
};

struct statistic {
	size_t lineNumber;
};
/* ------------------------------------- */

static struct options OPTS;
static struct statistic STAT;

void usage();
const char *getHost(const char *url, size_t len);
const unsigned char *getHash(const char *string, int string_len);
int getStdinLine(char *buf, int buf_size, char **line_start, int *line_len);
void onSignal(int sig);
void onAlarm(int sig);
size_t parseSize(const char *str);
void mainLoop(UniqueBTree &tree);

int main(int argc, char *argv[]) {
	const char *filename = "";
	unsigned long blockSize;
	unsigned long keyField;
	size_t cacheSize;
	unsigned long prefetchSize;

	char ch;

	STAT.lineNumber = 0;

	OPTS.blockSize = 1024*8;
	OPTS.forceCreateNewDb = 0;
	OPTS.verbose = 0;
	OPTS.urlMode = 0;
	OPTS.cacheSize = SIZE_T_MAX;

	// fields control
	OPTS.keyField   = 0;
	OPTS.keyFieldSeparator = '\t';

	while ((ch = getopt(argc, argv, "cvub:t:f:d:m:p:")) != -1) {
		switch (ch) {
			case 'b':
				blockSize = strtoul(optarg, NULL, 0);
				if(blockSize < 32 || blockSize == ULONG_MAX)
					fatalInUserOptions("Block size must be >32\n");

				OPTS.blockSize = blockSize;
			break;
			case 't':
				filename = optarg;
			break;
			case 'c':
				OPTS.forceCreateNewDb = 1;
			break;
			case 'u':
				OPTS.urlMode = 1;
			break;
			case 'v':
				OPTS.verbose = 1;
			break;
			case 'f':
				keyField = strtoul(optarg, NULL, 0);

				if(keyField == ULONG_MAX || keyField > INT_MAX)
					fatalInUserOptions("Field must be int");

				OPTS.keyField = keyField;
			break;
			case 'm':
				cacheSize = parseSize(optarg);

				if(cacheSize == SIZE_T_MAX)
					fatalInUserOptions("Cache size must be positive");

				OPTS.cacheSize = cacheSize;
			break;
			case 'p':
				prefetchSize = strtoul(optarg, NULL, 0);

				if(prefetchSize == ULONG_MAX || prefetchSize > SIZE_T_MAX || prefetchSize == 0)
					fatalInUserOptions("Prefetch size must be positive");

				OPTS.prefetchSize = (size_t)prefetchSize;
			break;
			case 'd':
				if(strlen(optarg) != 1)
					fatalInUserOptions("Field separator must be char");

				OPTS.keyFieldSeparator = *(char *)optarg;
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

	UniqueBTree tree(filename);
	tree.storage.setPrefetchSize(OPTS.prefetchSize);

	if(access(filename, R_OK | W_OK) == 0 && !OPTS.forceCreateNewDb) {
		tree.load();

		if(OPTS.verbose)
			fprintf(stderr, "Btree from %s with blockSize=%u was loaded\n", filename, (unsigned int)tree.blockSize);
	} else {
		tree.create(OPTS.blockSize);

		if(OPTS.verbose)
			fprintf(stderr, "New btree in %s with blockSize=%u was created\n", filename, (unsigned int)tree.blockSize);
	}

	if(OPTS.verbose)
		onAlarm(SIGALRM);

	setlinebuf(stdout);

	if(OPTS.cacheSize < tree.blockSize) {
		fprintf(stderr, "Cache size must be >=blockSize [%u]\n", (unsigned int)tree.blockSize);
		exit(255);
	}

	tree.storage.setCacheSize(OPTS.cacheSize / tree.blockSize);

	mainLoop(tree);

	return EXIT_SUCCESS;
}

void mainLoop(UniqueBTree &tree) {
	char *keyPtr;
	ssize_t keyLen;
	TokenReader reader(STDIN_FILENO);

	OPTS.reader = &reader;

	signal(SIGHUP, onSignal);
	signal(SIGINT, onSignal);
	signal(SIGKILL, onSignal);
	signal(SIGPIPE, onSignal);
	signal(SIGTERM, onSignal);

	signal(SIGALRM, onAlarm);


	char *linePtr = NULL;
	ssize_t lineLen;

	while((lineLen = reader.readUpToDelimiter('\n', (void **)&linePtr))) {
		if(lineLen < 0)
			fatal("Unable to read line from stdin");

		char *lineEnd = (linePtr + lineLen);

		if(OPTS.keyField == -1) {
			keyPtr = linePtr;
			keyLen = lineLen;
		} else { // need to cut key field
			keyPtr = linePtr;
			keyLen = -1;

			for(int curField = 0; curField < OPTS.keyField - 1; curField++) {
				keyPtr = (char *)memchr(keyPtr, OPTS.keyFieldSeparator, lineEnd - keyPtr);

				if(!keyPtr) {
					keyLen = 0;
					break;
				}

				keyPtr++;
			}

			if(keyLen) {
				char *keyEnd = (char *)memchr(keyPtr, OPTS.keyFieldSeparator, lineEnd - keyPtr);

				if(!keyEnd) { // последний филд в строке
					keyLen = lineEnd - keyPtr;
				} else {
					keyLen = keyEnd - keyPtr;
				}

				keyPtr[keyLen] = 0;
			} else { // нужного филда нет в строке

			}
		}

		if(tree.add(getHash(keyPtr, keyLen)))
			fwrite(linePtr, lineLen, 1, stdout);
	}

	OPTS.reader = NULL;
}

void onSignal(int sig) {
	if(OPTS.reader)
		OPTS.reader->setEof();
}

void onAlarm(int sig) {
	static double lastCallTime = -1;
	static size_t lastCallLineNumber = -1;
	static double firstCallTime = gettimed();
	static size_t firstCallLineNumber = STAT.lineNumber;

	if(lastCallTime > 0) {
		fprintf(
			stderr,
			"\rSpeed [i/s]: %u avg, %u cur                  ",
			(unsigned int)((STAT.lineNumber - firstCallLineNumber) / (gettimed() - firstCallTime)),
			(unsigned int)((STAT.lineNumber - lastCallLineNumber) / (gettimed() - lastCallTime))
		);
	}

	lastCallLineNumber = STAT.lineNumber;
	lastCallTime = gettimed();
	alarm(1);
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
	fputs("Usage: uq [-ucv] [-f N] [-d C] [-b N] [-f S] [-p S] -t storage\n", stderr);
	fputs("  -t <path>: path to storage\n", stderr);
	fputs("  -c: force creation of storage\n", stderr);
	fputs("  -u: url mode\n", stderr);
	fputs("  -v: verbose\n", stderr);
	fputs("  -f <number>: select key field\n", stderr);
	fputs("  -d <char>: use given delimiter instead of TAB for field delimiter\n", stderr);
	fputs("  -b <number>: block size\n", stderr);
	fputs("  -f <size>: cache size\n", stderr);
	fputs("  -p <size>: buffer prefetch size\n", stderr);
}

const unsigned char *getHash(const char *string, int string_len) {
	static unsigned char hashBuf[32];

	string = string;

	if(OPTS.urlMode) {
		const char *host = getHost(string, string_len);
		MD5((const unsigned char *)host, strlen(host), hashBuf);
		MD5((const unsigned char *)string, string_len, hashBuf + 3);
	} else {
		MD5((const unsigned char *)string, string_len, hashBuf);
	}

	return hashBuf;
}

size_t parseSize(const char *str) {
	char mul[] = {'b', 'k', 'm', 'g', 't', 'p', 'e', 'z', 'y'};
	char *inv;
	unsigned long l = strtoul(str, &inv, 0);

	if(l == ULONG_MAX)
		return SIZE_T_MAX;

	if(*inv != '\0') {
		size_t i;
		bool founded = false;
		for(i=0; i<sizeof(mul); i++) {
			if(tolower(*inv) == mul[i]) {
				l <<= 10 * i;
				founded = true;
				break;
			}
		}

		if(!founded)
			return SIZE_T_MAX;

		if(*(inv + 1) != '\0' && tolower(*(inv + 1)) != 'b')
			return SIZE_T_MAX;
	}

	if(l > SIZE_T_MAX)
		return SIZE_T_MAX;

	return (size_t)l;
}

/* THE END */
