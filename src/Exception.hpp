#ifndef EXCEPTION_HPP
#define	EXCEPTION_HPP

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <string>

class Exception {
public:
	std::string description;

	Exception(std::string description) {
		this->description = description;
	}

	void print(FILE *stream) const {
		fprintf(stream, "Error: %s\n", this->description.c_str());
	}
};

#endif	/* EXCEPTION_HPP */
