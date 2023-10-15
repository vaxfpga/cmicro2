// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "io.h"

#include "ucode.h"
#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static FILE *fin;
static FILE *flist;

static uint fpos[MAXUCODE];

bool io_get_line(char *buf, uint max)
{
    if (!fin)
        return false;

    if (max < 2)
    {
        ERROR_LINE("exceeded max line length!\n");
        return false;
    }

    buf[max-2] = '\n'; // to verify complete line fits
    const char *ret = fgets(buf, max, fin);
    if (!ret)
        return false;

    if (buf[max-2] != '\n')
    {
        ERROR_LINE("truncated line!\n");
        return false;
    }

    ++line_number;

    const char *nl = "";
    if (buf[strlen(buf)-1] != '\n')
        nl = "\n";

    DEBUG_LINES("line %-5u: %s%s", line_number, buf, nl);

    if (flist)
    {
        fprintf(flist, "; %5u: %s%s", line_number, buf, nl);
    }

    return true;
}

bool io_process_input(const char *fname)
{
    DEBUG_FILES("begin processing file %s\n", fname);

    if (flist)
        fprintf(flist, "; ---- begin file \"%s\" ----\n;\n", fname);

    line_number = 0;
    num_errors = 0;

    fin = fopen(fname, "r");
    if (!fin)
    {
        ERROR("failed to open file \"%s\"\n", fname);
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

    if (flist)
        fprintf(flist, ";\n; ---- end file \"%s\", %u lines, %u errors ----\n", fname, line_number, num_errors);

    fclose(fin);
    return true;
}

bool io_open_list(const char *fname)
{
    DEBUG_FILES("opening listing file %s\n", fname);

    flist = fopen(fname, "w+");
    if (!flist)
    {
        ERROR("failed to open file \"%s\"\n", fname);
        return false;
    }

    return true;
}

bool io_write_uc_placeholder(uint ucode_idx)
{
    if (!flist)
        return true;

    if (ucode_idx >= MAXUCODE)
        return false;

    long story = ftell(flist);
    fpos[ucode_idx] = story;

    fprintf(flist, "U,0000, 0000,0000,0000,0000,0000,0000\n");

    return true;
}

bool io_write_expanded(const char *xline)
{
    if (!flist)
        return true;

    if (!xline)
        return false;

    fprintf(flist, ";         \"%s\"\n", xline);

    return true;
}

bool io_write_error_list(const char *fmt, ...)
{
    if (!flist)
        return true;

    va_list args;
    va_start(args, fmt);
    fprintf(flist, "; ERROR: ");
    vfprintf(flist, fmt, args);
    return true;
}

bool io_update_close_list(void)
{
    if (!flist)
        return true;

    for (uint i=0; i<ucode_num; ++i)
    {
        if (!fpos[i])
            continue;

        fseek(flist, fpos[i], SEEK_SET);
        fprintf(flist, "U,%04X, %04X,%04X,%04X,%04X,%04X,%04X\n", ucode[i].addr,
            ucode[i].uc[2]>>16, ucode[i].uc[2] & 0xffff,
            ucode[i].uc[1]>>16, ucode[i].uc[1] & 0xffff,
            ucode[i].uc[0]>>16, ucode[i].uc[0] & 0xffff);
    }

    fclose(flist);
    return true;
}

bool io_write_symbol_list(void)
{
    if (!flist)
        return true;

    uint num_syms = 0;
    sym_pair_t *syms = util_get_symbols(&num_syms);
    if (syms)
    {
        uint max = 8;
        uint num_good_syms = 0;
        for (uint i=0; i<num_syms; ++i)
        {
            if (syms[i].addr == UCODE_UNALLOCATED)
                continue;
            uint len = strlen(syms[i].name);
            if (len > max)
                max = len;
            ++num_good_syms;
        }
        max = (max + 3) & ~3;

        fprintf(flist, "; ---- begin symbol table ----\n; %u symbols\n", num_good_syms);

        for (uint i=0; i<num_syms; ++i)
        {
            if (syms[i].addr == UCODE_UNALLOCATED)
                continue;
            fprintf(flist, ";   %-*s : 0x%04x\n", max, syms[i].name, syms[i].addr);
        }
    }

    fprintf(flist, ";\n; ---- end symbol table ----\n");
    return true;
}

bool io_write_bin(const char *fname)
{
    DEBUG_FILES("begin writing output file %s\n", fname);

    FILE *f = fopen(fname, "wb");
    if (!f)
    {
        ERROR("failed to open file \"%s\"\n", fname);
        return false;
    }

    uint32_t zeros[3] = { };
    for (uint i=0; i<=MAXPC; ++i)
    {
        if (!ucode_alloc[i])
            fwrite(zeros, 12, 1, f);
        else // TODO: assumes little-endian
            fwrite(ucode_alloc[i]->uc, 12, 1, f);
    }

    DEBUG_FILES("end writing output file %s\n", fname);

    fclose(f);

    return true;
}
