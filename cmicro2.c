// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

//#include <assert.h>
//#include <ctype.h>
#include <stdbool.h>
//#include <stdint.h>
#include <stdio.h>
//#include <string.h>

#include "constraints.h"
#include "fields.h"
#include "macros.h"
#include "hashtable.h"
#include "parser.h"
#include "utils.h"



static FILE *f;

bool io_get_line(char *buf, uint max)
{
    if (!f)
        return false;

    if (max < 2)
    {
        ERROR("exceeded max line length!\n");
        return false;
    }

    buf[max-2] = '\n'; // to verify complete line fit
    const char *ret = fgets(buf, max, f);
    if (!ret)
        return false;

    if (buf[max-2] != '\n')
    {
        ERROR("truncated line!\n");
        return false;
    }

    return true;
}

int process_file(const char *fname)
{
    DEBUG("begin processing file %s\n", fname);

    f = fopen(fname, "r");
    if (!f)
    {
        ERROR("failed to open file \"%s\"\n", fname);
        return -1;
    }

    char line[4096];
    while (true)
    {
        if (!get_logical_line(line, sizeof(line)))
            break;

        parse_line(line);
    }

    DEBUG("end processing file %s\n", fname);
    return 0;
}

int main(int argc, char *argv[])
{
    if (!fields_init())
        return 1;

    if (!macros_init())
        return 1;

//    constraint_t c;
//    constraint_parse(&c, "=0101");
//    uint32_t base = 0225;
//    uint32_t addr = base;
//    printf("0b%08b\n", addr); addr=constraint_next(&c, 0, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, 0, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, 0, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, 0, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, 0, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, 0, base, addr);
//
//    constraint_parse(&c, "=01*1*01");
//    constraint_t nc;
//    constraint_parse(&nc, "=1*");
//    base = addr = 0b10111001;
//    printf("0b%08b\n", addr); addr=constraint_next(&c, &nc, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, &nc, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, &nc, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, &nc, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, &nc, base, addr);
//    printf("0b%08b\n", addr); addr=constraint_next(&c, &nc, base, addr);

    while (*++argv)
    {
        process_file(*argv);
    }

    extern hashtable_t macros;
    for (uint i=0; i<macros.mask+1; ++i)
    {
        if (!macros.table[i].key)
            continue;
        printf("macro: %s \"%s\"\n", macros.table[i].key, macros.table[i].value_ptr);
    }

    return 0;
}
