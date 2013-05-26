#ifndef ERRNOEXCEPTION_HPP
#define	ERRNOEXCEPTION_HPP

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <string>

#include "Exception.hpp"

class ErrnoException: public Exception {
public:
	int errorCode;
	const char *errorString;

	ErrnoException(std::string description):
		Exception(description)
	{
		this->errorCode = errno;
		this->errorString = strerror(errorCode);
	}

	void print(FILE *stream) const {
		fprintf(stream, "Error: %s [#%d]: %s\n", this->errorString, (int)this->errorCode, this->description.c_str());
	}
};

#endif	/* ERRNOEXCEPTION_HPP */
