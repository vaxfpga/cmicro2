// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "constraints.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>


uint32_t constraint_next(const constraint_t *cst, const constraint_t *inner_cst, uint32_t base, uint32_t cur_addr)
{
    uint32_t next_addr = cur_addr;
    while (true)
    {
        next_addr = (((next_addr | cst->cmask) + cst->incr) & cst->mmask) | base;

        if (next_addr == base || next_addr == cur_addr)
            return CONSTRAINT_SET_FINISHED;
        else if (!inner_cst || constraint_matches(inner_cst, next_addr))
            return next_addr;
    }
}

const char *constraint_text(char *str, uint max, const constraint_t *cst)
{
    char *p = str;
    if (max < 2)
        return 0;

    *p++ = '=', --max;
    uint i=32;
    while (max >= 2 && i-- > 0)
    {
        if ((cst->vmask & (1<<i)) != 0)
            *p++ = '1', --max;
        else if ((cst->mmask & (1<<i)) != 0)
            *p++ = '0', --max;
        else if ((cst->cmask & (1<<i)) != 0)
            *p++ = '*', --max;
    }
    *p++ = 0;
    return str;
}
