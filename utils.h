// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_UTILS_H)
#define INCLUDED_UTILS_H

#include <stdio.h>

typedef unsigned int uint;

#define DEBUG(...) fprintf(stderr, "DEBUG: " __VA_ARGS__)
#define ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

#endif // INCLUDED_UTILS_H
