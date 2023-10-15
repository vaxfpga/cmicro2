// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

//#include <assert.h>
//#include <ctype.h>
#include <stdbool.h>
//#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
//#include <string.h>

#include "constraints.h"
#include "fields.h"
#include "macros.h"
#include "hashtable.h"
#include "parser.h"
#include "ucode.h"
#include "utils.h"

uint line_number = 0;
uint num_errors = 0;

static FILE *f;

static const char *listing_fname;
static const char *output_fname;

bool io_get_line(char *buf, uint max)
{
    if (!f)
        return false;

    if (max < 2)
    {
        ERROR_LINE("exceeded max line length!\n");
        return false;
    }

    buf[max-2] = '\n'; // to verify complete line fits
    const char *ret = fgets(buf, max, f);
    if (!ret)
        return false;

    if (buf[max-2] != '\n')
    {
        ERROR_LINE("truncated line!\n");
        return false;
    }

    ++line_number;
    DEBUG_LINES("line %-5u: %s", line_number, buf);

    return true;
}

bool process_file(const char *fname)
{
    DEBUG_FILES("begin processing file %s\n", fname);
    line_number = 0;
    num_errors = 0;

    f = fopen(fname, "r");
    if (!f)
    {
        ERROR_LINE("failed to open file \"%s\"\n", fname);
        return false;
    }

    char line[4096];
    while (get_logical_line(line, sizeof(line)))
    {
        parse_line(line);
    }

    DEBUG_FILES("end processing file %s\n", fname);

    if (num_errors > 0)
        ERROR("%u errors found in file %s\n", num_errors, fname);

    return true;
}

void print_usage(void)
{
    printf("usage: cmicro2 [options] [--] <infile.mic> [<infile2.mic>]...\n\n");
    printf("options:\n");
    printf("    -h, --help                    prints usage and exits\n");
    printf("    -d, --debug 0xNN              set debug flags to hex or decimal arg\n");
    printf("    -l, --listing <listfile.txt>  specify listing file (optional)\n");
    printf("    -o, --output <outfile.bin>    specify output bin file (mandatory)\n\n");
}

bool process_args(char *argv[])
{
    uint fc=0;
    while (*++argv)
    {
        if (strcmp(*argv, "--") == 0)
        {
            ++argv;
            break;
        }
        else if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0)
        {
            print_usage();
            return false;
        }
        else if (strcmp(*argv, "-d") == 0 || strcmp(*argv, "--debug") == 0)
        {
            ++argv;
            if (!*argv)
            {
                ERROR("cmicro2 -d option requires an argument\n");
                return false;
            }
            char *q = 0;
            debug_flags = strtoul(*argv, &q, 0);
            if (q == *argv)
            {
                ERROR("cmicro2 -d option requires numeric (NN or 0xNN) argument\n");
                return false;
            }
        }
        else if (strcmp(*argv, "-l") == 0 || strcmp(*argv, "--listing") == 0)
        {
            ++argv;
            if (!*argv)
            {
                ERROR("cmicro2 -l option requires an argument\n");
                return false;
            }
            listing_fname = *argv;
        }
        else if (strcmp(*argv, "-o") == 0 || strcmp(*argv, "--output") == 0)
        {
            ++argv;
            if (!*argv)
            {
                ERROR("cmicro2 -o option requires an argument\n");
                return false;
            }
            output_fname = *argv;
        }
        else if (**argv == '-')
        {
            ERROR("cmicro2 unknown option %s\n", *argv);
            return false;
        }
        else
            ++fc;
    }
    while (*argv++)
        ++fc;

    if (fc < 1)
    {
        ERROR("cmicro2 requires one or more input files\n");
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    if (!process_args(argv))
        return 1;

    if (!ucode_init())
        return 1;

    if (!fields_init())
        return 1;

    if (!macros_init())
        return 1;

    // process non-options as input files
    while (*++argv)
    {
        if (strcmp(*argv, "--") == 0)
        {
            ++argv;
            break;
        }
        else if (**argv == '-')
        {
            ++argv;
            continue;
        }

        process_file(*argv);
    }

    // process everything after -- as input files
    while (*argv)
        process_file(*argv++);

    if (!ucode_allocate())
    {
        ERROR("failure to allocate all ucode addresses\n");
    }

    if (!ucode_resolve())
    {
        ERROR("failure to resolve all ucode symbol references\n");
    }



    uint num_syms = 0;
    sym_pair_t *syms = util_get_symbols(&num_syms);
    if (syms)
    {
        printf("number of symbols: %u\n", num_syms);
        for (uint i=0; i<num_syms; ++i)
            printf("%-16s : 0x%04x\n", syms[i].name, syms[i].addr);
    }

//    extern ucode_inst_t *ucode_alloc[MAXPC];

    printf("---- memory order ----\n");
    for (uint i=0; i<=MAXPC; ++i)
    {
        if (!ucode_alloc[i])
            continue;
        printf("U,%04X, %04X,%04X,%04X,%04X,%04X,%04X\n", i,
            ucode_alloc[i]->uc[2]>>16, ucode_alloc[i]->uc[2] & 0xffff,
            ucode_alloc[i]->uc[1]>>16, ucode_alloc[i]->uc[1] & 0xffff,
            ucode_alloc[i]->uc[0]>>16, ucode_alloc[i]->uc[0] & 0xffff
        );
    }

//    extern ucode_inst_t ucode[MAXUCODE];
//    extern uint ucode_num;

    printf("---- code order ----\n");
    for (uint i=0; i<ucode_num; ++i)
    {
        if (ucode[i].cst)
            continue;
        printf("U,%04X, %04X,%04X,%04X,%04X,%04X,%04X\n", ucode[i].addr,
            ucode[i].uc[2]>>16, ucode[i].uc[2] & 0xffff,
            ucode[i].uc[1]>>16, ucode[i].uc[1] & 0xffff,
            ucode[i].uc[0]>>16, ucode[i].uc[0] & 0xffff
        );
    }

//    extern hashtable_t symbols;
//    for (uint i=0; i<symbols.mask+1; ++i)
//    {
//        if (!symbols.table[i].key)
//            continue;
//        ucode_inst_t *inst = symbols.table[i].value_ptr;
//        if (!inst)
//            printf("undefined symbol %s\n", symbols.table[i].key);
//        else
//            printf("symbols: %-16s : 0x%04x\n", symbols.table[i].key, inst->addr);
//
//    }
//    extern hashtable_t macros;
//    for (uint i=0; i<macros.mask+1; ++i)
//    {
//        if (!macros.table[i].key)
//            continue;
//        printf("macro: %s \"%s\"\n", macros.table[i].key, macros.table[i].value_ptr);
//    }

    return 0;
}
