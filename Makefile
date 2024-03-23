# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

all: cmicro2

#coverage: gcov/coverage.html

clean:
	$(RM) -f cmicro2 cmicro2-gcov test_hash *.o
	$(RM) -fr gcov/
	$(RM) -fr runs/

.PHONY: all clean coverage tests

CFLAGS += -Wall

SOURCES = \
    cmicro2.c \
    constraints.c \
    fields.c \
    hashtable.c \
    io.c \
    macros.c \
    region.c \
    parser.c \
    ucode.c \
    utils.c

MAKE_DIR=$(dir $(realpath $(lastword $(MAKEFILE_LIST))))

cmicro2: $(SOURCES) *.h
	$(CC) $(CFLAGS) -g -o $@ $(SOURCES)
    
test_hash: test_hash.c hashtable.c
	$(CC) $(CFLAGS) -g -o $@ $^

cmicro2-gcov: $(SOURCES) *.h
	mkdir -p gcov
	$(CC) $(CFLAGS) -fprofile-dir=gcov/ -fprofile-prefix-path=$(MAKE_DIR) --coverage -g -o $@ $(SOURCES)
	mv -f *.gcno gcov/

tests: cmicro2 *.mic run-tests.sh
	rm -fr runs/*
	./run-tests.sh

coverage: cmicro2-gcov *.mic run-tests.sh
	rm -fr gcov/*.gcda runs/*
	./run-tests.sh ./cmicro2-gcov
	gcovr --html-details gcov/coverage.html
