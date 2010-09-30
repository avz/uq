#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include <openssl/md5.h>

#include "storage.hpp"
#include "btree.hpp"
#include "btree_node.hpp"

/* ------------------------------------- */

int main(int argc, char *argv[]) {
	char line[1024];
	const char *filename = argv[1];


	if(argc != 2)
		return 255;

	UniqueBTree tree(filename);

	if(access(filename, R_OK | W_OK) == 0)
		tree.load();
	else
		tree.create();
/*
	for(unsigned int i=0; i<1000000; i++) {
		tree.add(MD5((unsigned char *)&i, sizeof(i), NULL));
	}*/

	while(fgets(line, 1024, stdin)) {
		if(tree.add(MD5((unsigned char *)line, strlen(line), NULL)))
			fputs(line, stdout);
	}

	return EXIT_SUCCESS;
}

/* THE END */
