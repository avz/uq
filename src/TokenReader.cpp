#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "TokenReader.hpp"

/**
 * Считать из потока кусок данных до разделителя, включая сам разделитель.
 * Если кусок не влез полностью во внутренний буфер, то в конце разделителя
 * не будет.
 * @param delimiter сивол-разделитель до которого читать
 * @param buf
 * @return кол-во считанных байт, 0 в случае EOF и -1 в случае ошибки (errno)
 */
ssize_t TokenReader::readUpToDelimiter(int delimiter, void **buf) {
	ssize_t readed = 0;

	/*
	 * Сколько байт от начала буффера мы уже проверили на делимитер.
	 * Используется чтобы не проверть ещё раз одно и то же после чтения
	 * очередного чанка
	 */
	ssize_t checkedOffset = 0;

	while(!this->eof) {
		char *c = (char *)memchr((void *)(this->readBufferStart + checkedOffset), delimiter, this->readBufferEnd - (this->readBufferStart + checkedOffset));
		if(c) {
			*buf = (void *)this->readBufferStart;

			readed = c - this->readBufferStart + 1;

			this->readBufferStart = c + 1;

			return readed;
		}

		checkedOffset = this->readBufferEnd - this->readBufferStart;

		if(this->readBufferStart >= this->readBufferEnd) {
			this->readBufferStart = this->readBuffer;
			this->readBufferEnd = this->readBuffer;
		}

		if(this->readBufferStart != this->readBuffer) {
			memmove(this->readBuffer, this->readBufferStart, this->readBufferEnd - this->readBufferStart);
			this->readBufferEnd -= (this->readBufferStart - this->readBuffer);
			this->readBufferStart = this->readBuffer;
		}

		/* while(EINTR) */
		while(true) {
			ssize_t r;

			r = read(this->fd, this->readBufferEnd, this->readBuffer + sizeof(this->readBuffer) - this->readBufferEnd);

//			fprintf(stderr, "%d, errno = %d\n", (int)this->eof, errno);

			if(r < 0 && !this->eof) {
				if(errno == EINTR)
					continue;

				return -1;
			} else if(r == 0 || this->eof) {
				*buf = (void *)this->readBufferStart;
				readed = this->readBufferEnd - this->readBufferStart;

				this->readBufferStart = this->readBuffer;
				this->readBufferEnd = this->readBuffer;

				return readed;
			}

			this->readBufferEnd += r;

			break;
		}
	}

	return 0; // EOF
}

void TokenReader::setEof() {
	this->eof = true;
}
