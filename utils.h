// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_UTILS_H)
#define INCLUDED_UTILS_H

#include <stdio.h>

#define MAXLINE 4096
#define MAXNAME 128
#define MAXARGS 9
#define MAXRECURSE 10

typedef unsigned int uint;

#define ENABLE_DEBUG 1
#if defined(ENABLE_DEBUG)
    #define DEBUG(...) fprintf(stderr, "DEBUG: " __VA_ARGS__)
#endif

#define ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

#endif // INCLUDED_UTILS_H
