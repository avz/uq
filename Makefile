CC=g++
LD=$(CC)
CFLAGS= -c -O2 -Wall -I/usr/local/include -g
LDFLAGS= -L/usr/local/lib
LIBS=-lssl

PROJ=uniq

OBJS=btree.o btree_node.o main.o misc.o storage.o

all: $(PROJ)

$(PROJ): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o "$(PROJ)"

.cpp.o:
	$(CC) $(CFLAGS) $*.cpp

clean:
	rm -f *.o "$(PROJ)"