CC=gcc
CFLAGS=-g -fPIC -Ideps -lm -lpcre -Wall -Wextra -pedantic -pthread
LDFLAGS=-shared -o
DEBUG=-DDEBUG=1 # TODO: use opt

BIN=libhttp.so
INTEG_BIN=integ

SRC=$(wildcard src/*.c)
DEPS=$(wildcard deps/*/*.c)
TESTS = $(patsubst %.c, %, $(wildcard t/*.c))

all:
	$(CC) $(DEBUG) $(CFLAGS) $(DEPS) $(SRC) $(LDFLAGS) $(BIN)

clean:
	rm -f $(SRC:.c=.o) $(BIN) main*

test:
	./scripts/test.bash
	$(MAKE) clean

integ_test:
	$(CC) t/integ/server.c -Isrc -Ideps -o $(INTEG_BIN) -L. -lhttp
	shpec

.PHONY: all clean test integ_test
