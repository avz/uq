#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <inttypes.h>
#include <openssl/md5.h>

#include "btree.hpp"

/* ------------------------------------- */
void usage();

int main(int argc, char *argv[]) {
	char line[1024];
	unsigned long blockSize = 4096*4;
	const char *filename = "";
	char forceCreate = 0;
	char ch;

	while ((ch = getopt(argc, argv, "cb:t:")) != -1) {
		switch (ch) {
			case 'b':
				blockSize = strtoul(optarg, NULL, 0);
				if(blockSize < 32 || blockSize == ULONG_MAX) {
					fputs("Block size mus be >32\n", stderr);
					exit(255);
				}
			break;
			case 't':
				filename = optarg;
			break;
			case 'c':
				forceCreate = 1;
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
		if(tree.add(MD5((unsigned char *)line, strlen(line), NULL)))
			fputs(line, stdout);
	}

	return EXIT_SUCCESS;
}

void usage() {
	fputs("Usage: uniq [-c] [-b blockSize] -t btreeFile\n", stderr);
}

/* THE END */
