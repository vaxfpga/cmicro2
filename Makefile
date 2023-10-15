# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

all: cmicro2

CFLAGS += -Wall

clean:
	$(RM) -f cmicro2 cmicro2-gcov test_hash *.o

SOURCES = \
    cmicro2.c \
    constraints.c \
    fields.c \
    hashtable.c \
    io.c \
    macros.c \
    parser.c \
    ucode.c \
    utils.c

cmicro2: $(SOURCES) *.h
	$(CC) $(CFLAGS) -g -o $@ $(SOURCES)
    
test_hash: test_hash.c hashtable.c
	$(CC) $(CFLAGS) -g -o $@ $^
