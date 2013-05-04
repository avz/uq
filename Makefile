CXX?=g++
LD=$(CXX)
CFLAGS?=-c -O2 -Wall -Werror -I/usr/local/include -g

LDFLAGS= -L/usr/local/lib

LIBS=-lssl -lcrypto

PROJECT=uq

OBJS=btree.o btree_node.o main.o misc.o storage.o token_reader.o

VPATH=src

build: $(PROJECT)

$(PROJECT): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o "$(PROJECT)"

.cpp.o:
	$(CXX) $(CFLAGS) src/$*.cpp

install: build
	install "$(PROJECT)" "$(PREFIX)/bin"

clean:
	rm -f *.o "$(PROJECT)"

test: build
	sh tests/run.sh
