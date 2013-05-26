CXX?=g++
LD=$(CXX)
CFLAGS?=-c -O2 -Wall -Werror -I/usr/local/include -g

LDFLAGS= -L/usr/local/lib

LIBS=

PROJECT=uq

OBJS=src/main.o \
    src/external/murmurhash3/MurmurHash3.o \
    src/blockStorage/backend/File.o \
    src/TokenReader.o

build: $(PROJECT)

$(PROJECT): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o "$(PROJECT)"

.cpp.o:
	$(CXX) $(CFLAGS) $*.cpp -o $*.o

install: build
	install "$(PROJECT)" "$(PREFIX)/bin"

clean:
	rm -f *.o */*.o */*/*.o */*/*/*.o "$(PROJECT)"

test: build
	sh tests/run.sh
