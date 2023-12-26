// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_IO_H)
#define INCLUDED_IO_H

#include "utils.h"

#include <stdbool.h>

typedef struct ucode_inst_s ucode_inst_t;

typedef struct file_info_s
{
    const char *filename;
    uint ucode_num;
} file_info_t;

bool io_get_line(char *buf, uint max);
bool io_process_hints(const char *fname);
bool io_process_input(const char *fname);

bool io_open_list(const char *fname);
bool io_write_uc_placeholder(uint ucode_idx);
bool io_write_expanded(const char *xline);
bool io_write_error_list(const char *fmt, ...);
bool io_write_symbol_list(void);
bool io_update_close_list(void);

bool io_write_hints(const char *output_fname, const file_info_t file_info[], uint num_files);

bool io_write_bin(const char *fname);

#endif // INCLUDED_IO_H
