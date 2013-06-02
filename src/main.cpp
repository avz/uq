#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "blockStorage/Storage.hpp"
#include "blockStorage/backend/File.hpp"
#include "ErrnoException.hpp"
#include "blockStorage/Superblock.hpp"

#include "TokenReader.hpp"
#include "Superblock.hpp"
#include "BTree.hpp"
#include "BTreeNode.hpp"

/**
100 Mln / ~108.5 Mln [============ 100% ==========] 525.2 Kln/s (ela 03m 10s)
pp -l /tmp/words  5,05s user 6,07s system 5% cpu 3:10,42 total
uq -ct /tmp/test.old > /dev/null  189,19s user 1,46s system 100% cpu 3:10,44 total
 */

int main(int argc, const char *argv[]) {
	try {
		int flags = O_RDWR | O_CREAT;
		blockStorage::backend::File backend("/tmp/test", flags);

		blockStorage::Storage<blockStorage::backend::File, BTreeNode, Superblock> storage(
			backend,
			16*1024*1024,
			flags
		);

		Superblock *sb = storage.getSuperblock();
		BTreeNode *root;

		if(!sb->getFirstBlockId()) {
			root = storage.allocate();
			root->makeLeaf();

			sb->setFirstBlockId(root->id);
		} else {
			root = storage.get(sb->getFirstBlockId());
		}

		BTree hash(storage);

//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0100000000000001ull));
//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0200000000000001ull));
//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0200000000000001ull));
//		fprintf(stderr, "RES %d\n", (int)hash.addKey(0x0000000000010101ull));
//return 0;
		void *buf;
		ssize_t bufLen;
		TokenReader reader(STDIN_FILENO);
		while((bufLen = reader.readUpToDelimiter('\n', &buf)) != 0) {
			if(bufLen < 0)
				throw ErrnoException(std::string("reading line from stdin"));

			if(hash.add(buf, bufLen))
				fwrite(buf, bufLen, 1, stdout);
		}
	} catch(const ErrnoException &e) {
		fprintf(stderr, "ERRNOEXCEPTION!!!\n");
		e.print(stderr);
		exit(e.errorCode);
	} catch(const Exception &e) {
		fprintf(stderr, "EXCEPTION!!!\n");
		e.print(stderr);
		exit(254);
	}

	return EXIT_SUCCESS;
}
