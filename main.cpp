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

int main(int argc, char *argv[]) {
	const char *filename = "";
	unsigned long blockSize;
	unsigned long keyField;
	size_t cacheSize;
	unsigned long prefetchSize;

	char ch;

	STAT.lineNumber = 0;

	OPTS.blockSize = 4096*2;
	OPTS.forceCreateNewDb = 0;
	OPTS.verbose = 0;
	OPTS.urlMode = 0;
	OPTS.cacheSize = SIZE_T_MAX;

	// fields control
	OPTS.keyField   = 0;
	OPTS.keyFieldSeparator      = ',';

	while ((ch = getopt(argc, argv, "sicvub:t:S:f:d:m:p:")) != -1) {
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

				OPTS.keyFieldSeparator      = *(char *)optarg;
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

	signal(SIGALRM, onAlarm);

	UniqueBTree tree(filename);
	tree.setPrefetchSize(OPTS.prefetchSize);

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

	setlinebuf(stdin);
	setlinebuf(stdout);

	char line[1024];
	char *linePtr;
	int   lineLen;

	if(OPTS.cacheSize < tree.blockSize) {
		fprintf(stderr, "Cache size must be >=blockSize [%u]\n", (unsigned int)tree.blockSize);
		exit(255);
	}

	tree.setCacheSize(OPTS.cacheSize / tree.blockSize);

	while(getStdinLine(line, sizeof(line), &linePtr, &lineLen)) {
		STAT.lineNumber++;

		if(tree.add(getHash(linePtr, lineLen)))
			fputs(line, stdout);
	}

	return EXIT_SUCCESS;
}

void onSignal(int sig) {
	fclose(stdin);
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

// returns 0 on EOF, 1 on success
int getStdinLine(char *buf, int bufSize, char **lineStart, int *lineLen){
	int curField;
	char *cur, *next;

	do {
		if(!fgets(buf, bufSize, stdin))
			return 0;

		if(OPTS.keyField == -1){
			*lineStart = buf;
			*lineLen = strlen(buf);
			return 1;
		}

		cur = buf;
		curField = 1;
		do {
			next = strchr(cur, OPTS.keyFieldSeparator);
			if(!next){
				if(curField == OPTS.keyField){
					*lineStart = cur;
					*lineLen = strlen(cur) - 1;
					return 1;
				}
				break;
			}

			if(curField == OPTS.keyField){
				*lineStart = cur;
				*lineLen = next - cur;
				return 1;
			}

			cur = next + 1; // skip field sep
			curField++;
		} while(cur);
	} while(1);
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
	fputs("Usage: uniq [-uc] -t btreeFile\n", stderr);
	fputs("\n", stderr);
	fputs("  -u        url mode\n", stderr);
	fputs("  -f        select field\n", stderr);
	fputs("  -d        use given delimiter instead of ';'\n", stderr);
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
