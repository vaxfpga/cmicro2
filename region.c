// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "region.h"

#include "utils.h"

static inline bool is_valid(const region_t *r, uint32_t a)
{
    return (a >= r->low_addr && a <= r->max_addr);
}

bool region_is_valid_addr(const region_list_t *rl, uint32_t a)
{
    for (uint i=0; i<rl->num_regions; ++i)
    {
        if (is_valid(&rl->regions[i], a))
            return true;
    }
    return false;
}

uint32_t region_init_addr(const region_list_t *rl, uint32_t hint)
{
    if (rl->num_regions <= 0)
        return REGION_FINISHED;

    if (region_is_valid_addr(rl, hint))
        return hint;

    return rl->regions[0].low_addr;
}

uint32_t region_next_addr(const region_list_t *rl, uint32_t a)
{
    for (uint i=0; i<rl->num_regions; ++i)
    {
        if (!is_valid(&rl->regions[i], a))
            continue;

        if (a < rl->regions[i].max_addr)
            return a+1;
        else if (i+1 < rl->num_regions)
            return rl->regions[i+1].low_addr;
        else
            break;
    }

    return REGION_FINISHED;
}
