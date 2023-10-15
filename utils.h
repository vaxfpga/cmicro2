// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_UTILS_H)
#define INCLUDED_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


#define MAXLINE 4096
#define MAXNAME 128

#define MAXARGS 9
#define MAXRECURSE 10

#define MAXFIELD 64

#define MAXUCODE  10000
#define MAXPC    0x1fff


typedef unsigned int uint;

extern uint line_number;
extern uint num_errors;

#if !defined(ENABLE_DEBUG)
    #define ENABLE_DEBUG 1
#endif

#define DEBUG_FILES(...)   DEBUG_FLAG(1, __VA_ARGS__)
#define DEBUG_LINES(...)   DEBUG_FLAG(2, __VA_ARGS__)
#define DEBUG_PARSING(...) DEBUG_FLAG(4, __VA_ARGS__)
#define DEBUG_MACROS(...)  DEBUG_FLAG(8, __VA_ARGS__)

#if ENABLE_DEBUG
    extern uint debug_flags;

    #define DEBUG_FLAG(flag, fmt, ...) do { if ((debug_flags & flag) != 0) fprintf(stderr, "DEBUG: " fmt, ##__VA_ARGS__); } while (0)
#else
    #define DEBUG_FLAG(flag, fmt, ...) do { } while(0)
#endif

extern bool io_write_error_list(const char *fmt, ...);
#define ERROR(fmt, ...)      do { fprintf(stderr, "ERROR: " fmt, ##__VA_ARGS__); } while (0)
#define ERROR_LINE(fmt, ...) do { ++num_errors; io_write_error_list(fmt, ##__VA_ARGS__); fprintf(stderr, "ERROR: line %u, " fmt, line_number, ##__VA_ARGS__); } while (0)


typedef struct
{
    const char *name;
    uint32_t addr;
} sym_pair_t;

sym_pair_t *util_get_symbols(uint *num);

#endif // INCLUDED_UTILS_H
