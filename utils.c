// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "utils.h"

#include "hashtable.h"
#include "ucode.h"

#include <stdlib.h>

extern hashtable_t symbols;

typedef bool (*handle_sym_t)(const char *name , uint32_t addr);



static int sym_compare(const void *a, const void *b)
{
    const sym_pair_t *sa = a;
    const sym_pair_t *sb = b;
    return strcmp(sa->name, sb->name);
}

sym_pair_t *util_get_symbols(uint *num)
{
    sym_pair_t *syms = calloc(symbols.mask+1, sizeof(sym_pair_t));
    if (!syms)
        return false;

    uint n = 0;
    for (uint i=0; i<symbols.mask+1; ++i)
    {
        if (!symbols.table[i].key)
            continue;
        ucode_inst_t *inst = symbols.table[i].value_ptr;
        uint32_t addr = inst ? inst->addr : UCODE_UNALLOCATED;
        syms[n++] = (sym_pair_t){ symbols.table[i].key, addr };
    }

    qsort(syms, n, sizeof(sym_pair_t), sym_compare);

    if (num)
        *num = n;
    return syms;
}
