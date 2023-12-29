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
uint total_errors = 0;

static const char *listing_fname;
static const char *output_fname;

static bool use_hints    = false;
static bool create_hints = false;

#define MAX_FILES 128
static file_info_t file_info[MAX_FILES];
static uint num_files = 0;

void print_usage(void)
{
    fprintf(stderr, "usage: cmicro2 [options] [--] <infile.mic> [<infile2.mic>]...\n\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "    -h, --help                    prints usage and exits\n");
    fprintf(stderr, "    -d, --debug 0xNN              set debug flags to hex or decimal arg\n");
    fprintf(stderr, "    -l, --listing <listfile.lst>  specify listing file (optional)\n");
    fprintf(stderr, "    -o, --output <outfile.bin>    specify output bin file (mandatory)\n");
    fprintf(stderr, "        --use-hints               use hint files, if they exist\n");
    fprintf(stderr, "        --create-hints            create hint files with current map\n\n");
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
        else if (strcmp(*argv, "--use-hints") == 0)
        {
            use_hints = true;
        }
        else if (strcmp(*argv, "--create-hints") == 0)
        {
            create_hints = true;
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

    if (create_hints && fc > MAX_FILES)
    {
        ERROR("too many input files (>%u)\n", MAX_FILES);
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
    uint last_ucode_num = 0;
    // process non-options as input files
    while (*++argv)
    {
        if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0 ||
            strcmp(*argv, "--use-hints") == 0 ||
            strcmp(*argv, "--create-hints") == 0)
        {
            continue;
        }
        else if (strcmp(*argv, "--") == 0)
        {
            get_arg(&argv);
            break;
        }
        else if (**argv == '-')
        {
            get_arg(&argv);
            continue;
        }

        if (use_hints)
        {
            if (!io_process_hints(*argv))
                (void)0; //++total_errors;
        }

        if (io_process_input(*argv))
            ++fc;
        else
            ++total_errors;

        if (use_hints)
        {
            if(!ucode_apply_hints())
                ++total_errors;
        }

        if (ucode_num > last_ucode_num)
        {
            last_ucode_num = ucode_num;
            file_info[num_files] = (file_info_t) {
                .filename  = *argv,
                .ucode_num = ucode_num
            };
            ++num_files;
        }
    }

    // process everything after -- as input files
    while (*argv)
    {
        if (use_hints)
        {
            if (!io_process_hints(*argv))
                ++total_errors;
        }

        if (io_process_input(*argv))
            ++fc;
        else
            ++total_errors;

        if (use_hints)
        {
            if(!ucode_apply_hints())
                ++total_errors;
        }

        if (ucode_num > last_ucode_num)
        {
            last_ucode_num = ucode_num;
            file_info[num_files] = (file_info_t) {
                .filename  = *argv,
                .ucode_num = ucode_num
            };
            ++num_files;
        }
        ++argv;
    }

    if (fc <= 0)
        return 1;

    num_errors = 0;

    if (!ucode_allocate())
    {
        ERROR("failure to allocate all ucode addresses\n");
        ++total_errors;
    }

    if (!ucode_resolve())
    {
        ERROR("failure to resolve all ucode symbol references\n");
        ++total_errors;
    }

    total_errors += num_errors;

    if (!io_write_symbol_list())
        ++total_errors;

    if (!io_update_close_list())
        ++total_errors;

    if (create_hints)
    {
        if (!io_write_hints(output_fname, file_info, num_files))
            ++total_errors;
    }

    if (!io_write_bin(output_fname))
        ++total_errors;

    if (total_errors > 0)
    {
        ERROR("%u total errors, failed\n", total_errors);
        return 1;
    }

    return 0;
}
