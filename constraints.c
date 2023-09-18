// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "constraints.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>

//bool constraint_parse(constraint_t *cst, const char *str)
//{
//    DEBUG("parsing constraint: %s\n", str);
//
//    *cst = (constraint_t) { };
//
//    if (str[0] != '=')
//        return false;
//
//    ++str;
//
//    while (str[0] == '0' || str[0] == '1' || str[0] == '*')
//    {
//        cst->incr  <<= 1;
//        cst->mmask <<= 1;
//        cst->cmask <<= 1;
//
//        if (str[0] == '0')
//        {
//            cst->incr = 1;
//            cst->mmask |= 1;
//        }
//        else if (str[0] == '1')
//        {
//            cst->mmask |= 1;
//            cst->cmask |= 1;
//        }
//        else if (str[0] == '*')
//        {
//            cst->cmask |= 1;
//        }
//
//        ++str;
//    }
//
//    cst->vmask = cst->mmask & cst->cmask;
//
//    DEBUG("parsed constraint: i=%08b v=%08b m=%08b c=%08b\n",
//          cst->incr, cst->vmask, cst->mmask, cst->cmask);
//
//    return (cst->mmask != 0 || cst->cmask != 0);
//}

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
