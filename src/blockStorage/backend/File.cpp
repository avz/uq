#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <exception>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "../../ErrnoException.hpp"
#include "File.hpp"

blockStorage::backend::File::File(const char *path, int flags):
	path(path),
	flags(flags)
{
	this->fd = open(path, this->flags, 0644);
	if(this->fd == -1)
		throw ErrnoException(std::string(path));
}

blockStorage::backend::File::~File() {
	if(this->fd >= 0) {
		close(this->fd);
		this->fd = -1;
	}
}

void *blockStorage::backend::File::read(off_t offset, ssize_t size) {
	void *buf;

//	fprintf(stderr, "Trying to read %llu bytes at %llu\n", (unsigned long long)size, (unsigned long long)offset);

	buf = malloc(size);

	try {
		ssize_t readed = 0;

		while(readed < size) {
			ssize_t r = pread(this->fd, (char *)buf + readed, size - readed, offset + readed);

			if(r == -1) {
				if(errno == EINTR)
					continue;

				throw ErrnoException(
					std::string("pread(")
					+ "'" + this->path + "'"
					+ ")"
				);
			} else if(r == 0) {
				throw ErrnoException(
					std::string("Unexpected EOF while pread('")
					+ "'" + this->path + "'"
					+ ")"
				);
			}

			readed += r;
		}
	} catch(const ErrnoException &e) {
		::free(buf);

		throw e;
	}

	return buf;
}

void blockStorage::backend::File::free(void *buf) {
	::free(buf);
}

void blockStorage::backend::File::write(const void *buf, off_t offset, ssize_t size) {
	ssize_t written = 0;

	while(written < size) {
		ssize_t w = pwrite(this->fd, (const char *)buf + written, size - written, offset + written);

		if(w == -1) {
			if(errno == EINTR)
				continue;

			throw ErrnoException(
				std::string("pwrite(")
				+ "'" + this->path + "'"
				+ ")"
			);
		} else if(w == 0) {
			throw ErrnoException(
				std::string("Unexpected EOF while pwrite('")
				+ "'" + this->path + "'"
				+ ")"
			);
		}

		written += w;
	}
}

off_t blockStorage::backend::File::allocate(ssize_t size) {
	off_t off = lseek(this->fd, 0, SEEK_END);
	if(off == (off_t)-1) {
		throw ErrnoException(
			std::string("lseek('")
			+ "'" + this->path + "'"
			+ "0, SEEK_END)"
		);
	}

	// preallocating
	do {
		int a = posix_fallocate(this->fd, off, size);
		if(a != 0) {
			if(errno != EINTR) {
				throw ErrnoException(
					std::string("posix_fallocate('")
					+ "'" + this->path + "'"
					+ "off, size)"
				);
			}
		} else {
			errno = 0;
		}
	} while(errno == EINTR);

//	fprintf(stderr, "Allocated %llu bytes at %llu\n", (unsigned long long)size, (unsigned long long)off);

	return off;
}

off_t blockStorage::backend::File::size() {
	off_t size = lseek(this->fd, 0, SEEK_END);
	if(size == (off_t)-1) {
		throw ErrnoException(
			std::string("lseek('")
			+ "'" + this->path + "'"
			+ "0, SEEK_END)"
		);
	}

	return size;
}

void blockStorage::backend::File::clear() {
	if(ftruncate(this->fd, 0) != 0) {
		throw ErrnoException(
			std::string("ftruncate()")
		);
	}
}

// -
