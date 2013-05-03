CC=g++
LD=$(CC)
CFLAGS?=-c -O2 -Wall -I/usr/local/include -g

LDFLAGS= -L/usr/local/lib

LIBS=-lssl -lcrypto

PROJECT=uq

OBJS=btree.o btree_node.o main.o misc.o storage.o

VPATH=src

build: $(PROJECT)

$(PROJECT): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o "$(PROJECT)"

.cpp.o:
	$(CC) $(CFLAGS) src/$*.cpp

install: build
	install "$(PROJECT)" "$(PREFIX)/bin"

clean:
	rm -f *.o "$(PROJECT)"
