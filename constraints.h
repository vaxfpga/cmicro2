// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_CONSTRAINTS_H)
#define INCLUDED_CONSTRAINTS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct constraint_s
{
    uint32_t incr;  // increment: (1 << n) where n is first 0 bit
    uint32_t vmask; // value mask: true for 1, false for 0/*
    uint32_t mmask; // match mask: true for 0/1, false for *
    uint32_t cmask; // carry mask: true for 1/*, false for 0
} constraint_t;

#define CONSTRAINT_SET_FINISHED (~(uint32_t)0)

bool constraint_parse(constraint_t *cst, const char *str);

static inline bool constraint_matches(const constraint_t *cst, uint32_t base)
{
    return (base & cst->mmask) == cst->vmask;
}

uint32_t constraint_next(constraint_t *cst, constraint_t *inner_cst, uint32_t base, uint32_t cur_addr);

#endif // INCLUDED_CONSTRAINTS_H
