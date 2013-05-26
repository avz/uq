#ifndef TOKENREADER_HPP
#define	TOKENREADER_HPP

class TokenReader {
	int fd;
	bool eof;

	char readBuffer[4096];
	char *readBufferStart;
	char *readBufferEnd;

public:
	TokenReader(int fd):
		fd(fd),
		eof(false),
		readBufferStart(readBuffer),
		readBufferEnd(readBuffer)
	{
	}

	/**
	 * Считать из потока кусок данных до разделителя, включая сам разделитель.
	 * Если кусок не влез полностью во внутренний буфер, то в конце разделителя
	 * не будет.
     * @param delimiter сивол-разделитель до которого читать
     * @param buf
     * @return кол-во считанных байт, 0 в случае EOF и -1 в случае ошибки (errno)
     */
	ssize_t readUpToDelimiter(int delimiter, void **buf);
	void setEof();
};

#endif	/* TOKENREADER_HPP */
