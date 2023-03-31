DEPS_DIR := deps
SRC_DIR := src
LIB_DIR := lib
TEST_DIR := t
UNIT_DIR := $(TEST_DIR)/unit
INTEG_DIR := $(TEST_DIR)/integ

CC := gcc
CFLAGS := -g -fPIC -Wall -Wextra -pedantic -Wno-missing-braces -I$(DEPS_DIR) -lm -lpcre -pthread
LDFLAGS := -shared

LINTER := clang-format
INTEG_RUNNER := shpec

BIN := libhttp.so
INTEG_BIN := integ

SRC := $(wildcard $(SRC_DIR)/*.c)
DEPS := $(wildcard $(DEPS_DIR)/*/*.c)
TESTS := $(wildcard $(TEST_DIR)/*/*.c)

ifdef USE_TLS
	CFLAGS += -lcrypto -I/usr/lib/openssl-1.1/ -lssl -DUSE_TLS=1
endif

ifdef DEBUG
	SRC += $(LIB_DIR)/debug.h
	CFLAGS += -I$(LIB_DIR)
endif

all:
	$(info USE_TLS=$(USE_TLS) DEBUG=$(DEBUG))
	$(CC) $(CFLAGS) $(DEPS) $(SRC) $(LDFLAGS) -o $(BIN)

clean:
	rm -f $(filter-out %.h, $(SRC:.c=.o)) $(BIN) integ

test:
	$(MAKE) unit_test
	$(MAKE) integ_test

unit_test:
	$(MAKE) all
	./scripts/test.bash
	$(MAKE) clean

# make -s integ_test 2>/dev/null
integ_test:
	$(MAKE) all # TODO: if not .so
	$(CC) $(INTEG_DIR)/server.c $(wildcard $(INTEG_DIR)/deps/*/*.c) -I$(SRC_DIR) -I$(DEPS_DIR) -L. -lhttp -o $(INTEG_BIN)
	$(INTEG_RUNNER)
	$(MAKE) clean

lint:
	$(LINTER) -i $(SRC) $(TESTS)

.PHONY: all clean test unit_test integ_test lint
