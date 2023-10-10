// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "constraints.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>


uint32_t constraint_next(constraint_t *cst, constraint_t *inner_cst, uint32_t base, uint32_t cur_addr)
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
