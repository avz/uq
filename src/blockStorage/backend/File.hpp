#ifndef BLOCKSTORAGE_BACKEND_FILE_HPP
#define	BLOCKSTORAGE_BACKEND_FILE_HPP
#include <sys/types.h>

#include "../Backend.hpp"

namespace blockStorage {
	namespace backend {
		class File: public blockStorage::Backend {
			const char *path;
			int flags;

			int fd;

		public:
			File(const char *path, int flags);
			~File();

			void *read(off_t offset, ssize_t size);
			void write(const void *buf, off_t offset, ssize_t size);

			void free(void *buf);

			off_t allocate(ssize_t size);

			off_t size();
			void clear();
		};
	}
}

#endif	/* BLOCKSTORAGE_BACKEND_FILE_HPP */

