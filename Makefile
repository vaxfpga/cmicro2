# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

all: cmicro2

clean:
	$(RM) -f cmicro2 cmicro2-gcov test_hash *.o

SOURCES = \
    cmicro2.c \
    constraints.c \
    fields.c \
    handlers.c \
    hashtable.c \
    macros.c \
    parser.c

cmicro2: $(SOURCES)
	$(CC) -g -o $@ $^
    
test_hash: test_hash.c hashtable.c
	$(CC) -g -o $@ $^
