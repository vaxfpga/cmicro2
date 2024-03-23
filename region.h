// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_REGION_H)
#define INCLUDED_REGION_H

#include <stdint.h>
#include <stdbool.h>

#include "utils.h"

#define REGION_FINISHED (~(uint32_t)0)

typedef struct region_s
{
    uint32_t low_addr;
    uint32_t max_addr;
} region_t;

typedef struct region_list_s
{
    region_t regions[4];
    uint num_regions;
} region_list_t;

bool region_is_valid_addr(const region_list_t *rl, uint32_t a);

uint32_t region_init_addr(const region_list_t *rl, uint32_t hint);
uint32_t region_next_addr(const region_list_t *rl, uint32_t a);

#endif // INCLUDED_REGION_H
