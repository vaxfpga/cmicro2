// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraints.h"
#include "fields.h"
#include "hashtable.h"
#include "io.h"
#include "macros.h"
#include "parser.h"
#include "ucode.h"
#include "utils.h"

uint line_number = 0;
uint num_errors = 0;

static const char *listing_fname;
static const char *output_fname;

void print_usage(void)
{
    printf("usage: cmicro2 [options] [--] <infile.mic> [<infile2.mic>]...\n\n");
    printf("options:\n");
    printf("    -h, --help                    prints usage and exits\n");
    printf("    -d, --debug 0xNN              set debug flags to hex or decimal arg\n");
    printf("    -l, --listing <listfile.lst>  specify listing file (optional)\n");
    printf("    -o, --output <outfile.bin>    specify output bin file (mandatory)\n\n");
}

static inline const char *get_arg(char ***argv) // yes, sigh
{
    // single letter options return part of current arg
    // otherwise argv is incremented
    if ((**argv)[0] == '-' && (**argv)[1] != '-' && (**argv)[2])
        return &(**argv)[2];
    ++*argv;
    return **argv ? **argv : 0;
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
        else if (strncmp(*argv, "-d",2) == 0 || strcmp(*argv, "--debug") == 0)
        {
            const char *p = get_arg(&argv);
            if (!p || *p == '-')
            {
                ERROR("cmicro2 -d option requires an argument\n");
                return false;
            }
            char *q = 0;
            debug_flags = strtoul(p, &q, 0);
            if (q == *argv)
            {
                ERROR("cmicro2 -d option requires numeric (NN or 0xNN) argument\n");
                return false;
            }
        }
        else if (strncmp(*argv, "-l", 2) == 0 || strcmp(*argv, "--listing") == 0)
        {
            const char *p = get_arg(&argv);
            if (!p || *p == '-')
            {
                ERROR("cmicro2 -l option requires an argument\n");
                return false;
            }
            listing_fname = p;
        }
        else if (strncmp(*argv, "-o", 2) == 0 || strcmp(*argv, "--output") == 0)
        {
            const char *p = get_arg(&argv);
            if (!p || *p == '-')
            {
                ERROR("cmicro2 -o option requires an argument\n");
                return false;
            }
            output_fname = p;
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

    if (!output_fname)
    {
        output_fname = "a.bin";
//        ERROR("cmicro2 requires output file name (-o or --output)\n");
//        return false;
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

    if (listing_fname && !io_open_list(listing_fname))
        return 1;

    uint fc = 0;
    // process non-options as input files
    while (*++argv)
    {
        if (strcmp(*argv, "--") == 0)
        {
            get_arg(&argv);
            break;
        }
        else if (**argv == '-')
        {
            get_arg(&argv);
            continue;
        }

        if (io_process_input(*argv))
            ++fc;
    }

    // process everything after -- as input files
    while (*argv)
    {
        if (io_process_input(*argv++))
            ++fc;
    }

    if (fc <= 0)
        return 1;

    if (!ucode_allocate())
    {
        ERROR("failure to allocate all ucode addresses\n");
    }

    if (!ucode_resolve())
    {
        ERROR("failure to resolve all ucode symbol references\n");
    }

    io_write_symbol_list();
    io_update_close_list();

    if (!io_write_bin(output_fname))
        return 1;

    return 0;
}
