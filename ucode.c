// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "ucode.h"

#include "constraints.h"
#include "fields.h"
#include "hashtable.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

hashtable_t symbols;

ucode_inst_t  ucode[MAXUCODE];
uint          ucode_num = 0;

ucode_inst_t *ucode_alloc[MAXPC+1];

static uint32_t ucode_addr = UCODE_UNALLOCATED;

static uint32_t ucode_region_low  = 0;
static uint32_t ucode_region_high = MAXPC;

static const char *next_addr_str = "<.+1>";
static const char *this_addr_str = "<.>";


bool ucode_init(void)
{
    ucode_num         = 0;
    ucode_addr        = UCODE_UNALLOCATED;
    ucode_region_low  = 0;
    ucode_region_high = MAXPC;

    memset(ucode_alloc, 0, sizeof(ucode_alloc));

    return hashtable_init(&symbols, 256);
}

bool handle_region(uint32_t low, uint32_t high)
{
    ucode_region_low  = low;
    ucode_region_high = high;
    return true;
}

bool handle_constraint(const constraint_t *cst)
{
    if (ucode_num >= MAXUCODE)
    {
        ERROR("too many ucodes\n");
        return false;
    }

    constraint_t *c = calloc(1, sizeof(constraint_t));
    if (!c)
    {
        ERROR("allocation failed (cst)\n");
        return false;
    }

    *c = *cst;

    ucode[ucode_num] = (ucode_inst_t) {
        .addr        = ucode_addr,
        .cst         = c,
        .target_addr = 0
    };

    ucode_addr = UCODE_UNALLOCATED;
    ++ucode_num;
    return true;
}

bool handle_addr(uint addr)
{
    if (ucode_addr == UCODE_UNALLOCATED)
        ucode_addr = addr;
    else if (addr > MAXPC)
    {
        ERROR("address 0x%04x out of range\n", addr);
        return false;
    }
    else
    {
        ERROR("multiple address specifiers\n");
        return false;
    }

    return true;
}

bool handle_label(const char *label)
{
    hashtable_entry_t *hte = hashtable_get_entry(&symbols, label);
    if (!hte)
        return hashtable_add(&symbols, label, &ucode[ucode_num]);
    else if (!hte->value_ptr)
    {
        hte->value_ptr = &ucode[ucode_num];
        return true;
    }

    ERROR("duplicate label: %s\n", label);
    return false;
}

static void apply_val(uint32_t uc[3], uint li, uint ri, uint32_t val)
{
    uint w = li - ri + 1;
    uint32_t mask = (UINT32_C(1)<<w)-1;

    val &= mask;

    if (ri >= 64) // 64 <= ri < 96
    {
        uc[2] |= (val<<(ri-64));
    }
    else if (ri >= 32) // 32 <= ri < 64
    {
        if (li >= 64)
        {
            apply_val(uc, li, 64, val>>(64-ri));
            apply_val(uc, 63, ri, val);
        }
        else
            uc[1] |= (val<<(ri-32));
    }
    else // 0 <= ri < 32
    {
        if (li >= 32)
        {
            apply_val(uc, li, 32, val>>(32-ri));
            apply_val(uc, 31, ri, val);
        }
        else
            uc[0] |= (val<<ri);
    }
}

static bool is_set(uint32_t def[3], uint li, uint ri)
{
    uint w = li - ri + 1;
    uint32_t mask = (UINT32_C(1)<<w)-1;

    if (ri >= 64) // 64 <= ri < 96
    {
        if ((def[2] & (mask<<(ri-64))) != 0)
            return true;
    }
    else if (ri >= 32) // 32 <= ri < 64
    {
        if (li >= 64)
        {
            if (is_set(def, li, 64) || is_set(def, 63, ri))
                return true;
        }
        else
        {
            if ((def[1] & (mask<<(ri-32))) != 0)
                return true;
        }
    }
    else // 0 <= ri < 32
    {
        if (li >= 32)
        {
            if (is_set(def, li, 32) || is_set(def, 31, ri))
                return true;
        }
        else
        {
            if ((def[0] & (mask<<ri)) != 0)
                return true;
        }
    }

    return false;
}

bool handle_ucode(const ucode_field_t *field, uint num)
{
    if (ucode_num >= MAXUCODE)
    {
        ERROR("too many ucodes\n");
        return false;
    }

    ucode[ucode_num] = (ucode_inst_t) {
        .addr        = ucode_addr,
        .cst         = 0,
        .target_addr = 0
    };

    uint32_t def[3] = { };

    // set defined fields
    for (uint i=0; i<num; ++i)
    {
        const field_def_t *fdef = field_def_get(field[i].name);
        if (!fdef)
        {
            ERROR("undefined field %s\n", field[i].name);
            return false;
        }

        uint32_t val = field[i].valint;
        if (field[i].valstr)
        {
            val = field_val_get(fdef, field[i].valstr);
            if (val == HASHTABLE_ENTRY_NOT_FOUND && !fdef->addr_flag)
            {
                ERROR("undefined field value %s\n", field[i].name);
                return false;
            }

            if (fdef->addr_flag)
            {
                if (strcmp(field[i].valstr, this_addr_str) == 0)
                    ucode[ucode_num].target_addr = this_addr_str;
                else
                {
                    hashtable_add(&symbols, field[i].valstr, 0);
                    hashtable_entry_t *hte = hashtable_get_entry(&symbols, field[i].valstr);
                    if (!hte)
                    {
                        ERROR("hashtable fail: %s\n", field[i].valstr);
                        return false;
                    }
                    ucode[ucode_num].target_addr = hte->key;
                }
                val = 0;
            }
        }

        apply_val(ucode[ucode_num].uc, fdef->li, fdef->ri, val);
        apply_val(def, fdef->li, fdef->ri, ~(uint32_t)0);
    }

    // set defaulted fields
    for (uint i=0; i<fields.mask+1; ++i)
    {
        if (!fields.table[i].key)
            continue;

        const field_def_t *fdef = fields.table[i].value_ptr;
        if (is_set(def, fdef->li, fdef->ri))
            continue;

        if (fdef->def_flag)
            apply_val(ucode[ucode_num].uc, fdef->li, fdef->ri, fdef->def_val);
        else if (fdef->next_flag)
            ucode[ucode_num].target_addr = next_addr_str;
    }

    ucode_addr = UCODE_UNALLOCATED;
    ++ucode_num;
    return true;
}

static uint32_t get_cst_base(const constraint_t *cst)
{
    for (uint32_t a=ucode_region_low; a <= ucode_region_high; ++a)
    {
        if (constraint_matches(cst, a))
        {
            uint32_t cur  = a;
            while (cur != CONSTRAINT_SET_FINISHED && !ucode_alloc[cur])
                cur = constraint_next(cst, 0, a, cur);

            if (cur == CONSTRAINT_SET_FINISHED)
                return a;
        }
    }
    ERROR("can't satisfy constraint\n");
    return CONSTRAINT_SET_FINISHED;
}

bool ucode_allocate(void)
{
    // allocate fixed
    for (uint i=0; i<ucode_num; ++i)
    {
        if (ucode[i].cst || ucode[i].addr == UCODE_UNALLOCATED)
            continue; // skip constraints and unallocated

        if (ucode_alloc[ucode[i].addr])
        {
            ERROR("duplicate address 0x%04x\n", ucode[i].addr);
            return false;
        }
        ucode_alloc[ucode[i].addr] = &ucode[i];
    }

    const constraint_t *cst = 0;
    uint32_t base = 0;
    uint32_t cur = 0;

    // allocate constrained
    for (uint i=0; i<ucode_num; ++i)
    {
        // process constraint
        if (ucode[i].cst)
        {
            if (constraint_is_terminator(ucode[i].cst))
            {
                cst = 0;
                cur = base = 0;
            }
            else if (!cst)
            {
                cst = ucode[i].cst;
                cur = base = get_cst_base(cst);
                if (base == CONSTRAINT_SET_FINISHED)
                    return false;
            }
            else
            {
                // inner-constraint may skip addrs
                if (!constraint_matches(ucode[i].cst, cur))
                    cur = constraint_next(cst, ucode[i].cst, base, cur);
            }

            continue;
        }
        else if (!cst || ucode[i].addr != UCODE_UNALLOCATED)
            continue; // skip outside constraint or allocated

        // allocate ucode!
        ucode_alloc[cur] = &ucode[i];
        ucode[i].addr = cur;

        cur = constraint_next(cst, 0, base, cur);
        if (cur == CONSTRAINT_SET_FINISHED)
        {
            cst = 0;
            cur = base = 0;
        }
    }

    // allocate rest
    for (uint i=0; i<ucode_num; ++i)
    {
        if (ucode[i].cst || ucode[i].addr != UCODE_UNALLOCATED)
            continue; // skip constraints and allocated

        uint32_t addr = UCODE_UNALLOCATED;
        for (uint32_t a = ucode_region_low; a <= ucode_region_high; ++a)
        {
            if (!ucode_alloc[a])
            {
                addr = a;
                break;
            }
        }

        if (addr == UCODE_UNALLOCATED)
        {
            ERROR("can't allocate, no free address\n");
            return false;
        }

        ucode_alloc[addr] = &ucode[i];
        ucode[i].addr = addr;
    }
}

bool ucode_resolve(void)
{
    const field_def_t *fdef = 0;
    for (uint i=0; i<fields.mask+1; ++i)
    {
        if (!fields.table[i].key)
            continue;

        const field_def_t *f = fields.table[i].value_ptr;
        if (f && f->addr_flag)
        {
            fdef = fields.table[i].value_ptr;
            break;
        }
    }

    for (uint i=0; i<ucode_num; ++i)
    {
        if (!ucode[i].target_addr)
            continue;

        if (strcmp(ucode[i].target_addr, this_addr_str) == 0)
            apply_val(ucode[i].uc, fdef->li, fdef->ri, ucode[i].addr);
        else if (strcmp(ucode[i].target_addr, next_addr_str) == 0)
        {
            uint j;
            for (j=i+1; j<ucode_num && ucode[j].cst; ++j);
            apply_val(ucode[i].uc, fdef->li, fdef->ri, ucode[j].addr);
        }
        else
        {
            ucode_inst_t *inst = hashtable_get(&symbols, ucode[i].target_addr);
            if (!inst || inst->addr == UCODE_UNALLOCATED)
            {
                ERROR("unresolved symbol %s\n", ucode[i].target_addr);
                return false;
            }
            apply_val(ucode[i].uc, fdef->li, fdef->ri, inst->addr);
        }
    }

    return true;
}
