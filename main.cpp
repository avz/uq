#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <inttypes.h>
#include <openssl/md5.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "btree.hpp"

/* ------------------------------------- */
void usage();
const char *getHost(const char *url);

int main(int argc, char *argv[]) {
	char line[1024];
	char keyBuf[32];
	unsigned long blockSize = 4096*4;
	const char *filename = "";
	char forceCreate = 0;
	char urlMode = 0;

	char ch;

	while ((ch = getopt(argc, argv, "cub:t:")) != -1) {
		switch (ch) {
			case 'b':
				blockSize = strtoul(optarg, NULL, 0);
				if(blockSize < 32 || blockSize == ULONG_MAX) {
					fputs("Block size must be >32\n", stderr);
					exit(255);
				}
			break;
			case 't':
				filename = optarg;
			break;
			case 'c':
				forceCreate = 1;
			break;
			case 'u':
				urlMode = 1;
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

	if(access(filename, R_OK | W_OK) == 0 && !forceCreate) {
		tree.load();
		fprintf(stderr, "Btree from %s with blockSize=%u was loaded\n", filename, tree.blockSize);
	} else {
		tree.create((size_t)blockSize);
		fprintf(stderr, "New btree in %s with blockSize=%u was created\n", filename, tree.blockSize);
	}
/*
	for(unsigned int i=0; i<1400; i++) {
		tree.add(MD5((unsigned char *)&i, sizeof(i), NULL));
	}*/
	setlinebuf(stdin);
	setlinebuf(stdout);

	while(fgets(line, 1024, stdin)) {
		if(urlMode) {
			const char *host = getHost(line);
			MD5((const unsigned char *)host, strlen(host), (unsigned char *)keyBuf);
			MD5((unsigned char *)line, strlen(line), (unsigned char *)keyBuf+3);
		} else {
			MD5((unsigned char *)line, strlen(line), (unsigned char *)keyBuf);
		}
		if(tree.add(keyBuf))
			fputs(line, stdout);
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

/* THE END */
