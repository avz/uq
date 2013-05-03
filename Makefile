CC=g++
LD=$(CC)
CFLAGS= -c -O2 -Wall -I/usr/local/include -g
LDFLAGS= -L/usr/local/lib
LIBS=-lssl -lcrypto

PROJ=uniq

OBJS=btree.o btree_node.o main.o misc.o storage.o

build: $(PROJ)

$(PROJ): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o "$(PROJ)"

.cpp.o:
	$(CC) $(CFLAGS) $*.cpp

clean:
	rm -f *.o "$(PROJ)"
