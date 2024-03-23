// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "ucode.h"

#include "constraints.h"
#include "fields.h"
#include "hashtable.h"
#include "region.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

hashtable_t symbols;

ucode_inst_t  ucode[MAXUCODE];
uint          ucode_num = 0;

ucode_inst_t *ucode_alloc[MAXPC+1];

uint32_t      ucode_hints[MAXUCODE];
uint          ucode_hint_type[MAXUCODE];
uint          ucode_num_hints = 0;

static uint   last_ucode_num = 0;

static bool ucode_xresv_cst[MAXPC+1];
static bool ucode_xresv_seq[MAXPC+1];

static bool ucode_xfill_define = false;
static bool ucode_xfill_valid  = false;
static ucode_inst_t ucode_xfill;

static uint32_t ucode_addr = UCODE_UNALLOCATED;
static uint32_t ucode_hint = UCODE_UNALLOCATED;

static region_list_t *region_list = &(region_list_t)
{
    .regions = { [0]{ .low_addr=0, .max_addr=MAXPC } },
    .num_regions = 1,
};

static const char *next_addr_str = "<.+1>";
static const char *this_addr_str = "<.>";


bool ucode_init(void)
{
    ucode_num         = 0;
    ucode_num_hints   = 0;
    last_ucode_num    = 0;
    ucode_addr        = UCODE_UNALLOCATED;
    ucode_hint        = UCODE_UNALLOCATED;

    memset(ucode_alloc,     0, sizeof(ucode_alloc));
    memset(ucode_xresv_cst, 0, sizeof(ucode_xresv_cst));
    memset(ucode_xresv_seq, 0, sizeof(ucode_xresv_cst));

    return hashtable_init(&symbols, 256);
}

bool handle_region(region_list_t *r)
{
    region_list_t *rl = calloc(1, sizeof(region_list_t));
    memcpy(rl, r, sizeof(region_list_t));
    region_list = rl;
    return true;
}

bool handle_constraint(const constraint_t *cst)
{
    if (ucode_num >= MAXUCODE)
    {
        ERROR_LINE("too many ucodes (cst)\n");
        return false;
    }

    constraint_t *c = calloc(1, sizeof(constraint_t));
    if (!c)
    {
        ERROR_LINE("allocation failed (cst)\n");
        return false;
    }

    *c = *cst;

    ucode[ucode_num] = (ucode_inst_t) {
        .addr        = ucode_addr,
        .hint        = ucode_hint,
        .cst         = c,
        .target_addr = 0,
        .file_name   = file_name,
        .line        = line_number,
    };

    ucode_addr = UCODE_UNALLOCATED;
    ucode_hint = UCODE_UNALLOCATED;
    ++ucode_num;
    return true;
}

bool handle_addr(uint addr)
{
    if (ucode_addr == UCODE_UNALLOCATED)
        ucode_addr = addr;
    else if (addr > MAXPC)
    {
        ERROR_LINE("address 0x%04x out of range\n", addr);
        return false;
    }
    else
    {
        ERROR_LINE("multiple address specifiers\n");
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

    ERROR_LINE("duplicate label: %s\n", label);
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
        ERROR_LINE("too many ucodes\n");
        return false;
    }

    ucode[ucode_num] = (ucode_inst_t) {
        .addr        = ucode_addr,
        .hint        = ucode_hint,
        .cst         = 0,
        .target_addr = 0,
        .file_name   = file_name,
        .line        = line_number,
    };

    uint32_t def[3] = { };

    // set defined fields
    for (uint i=0; i<num; ++i)
    {
        const field_def_t *fdef = field_def_get(field[i].name);
        if (!fdef)
        {
            ucode_addr = UCODE_UNALLOCATED;
            ERROR_LINE("undefined field %s\n", field[i].name);
            //return false;
            break;
        }

        uint32_t val = field[i].valint;
        if (field[i].valstr)
        {
            val = field_val_get(fdef, field[i].valstr);
            if (val == HASHTABLE_ENTRY_NOT_FOUND && !fdef->addr_flag)
            {
                ucode_addr = UCODE_UNALLOCATED;
                ERROR_LINE("undefined field value %s\n", field[i].valstr);
                //return false;
                break;
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
                        ucode_addr = UCODE_UNALLOCATED;
                        ERROR_LINE("hashtable fail: %s\n", field[i].valstr);
                        //return false;
                        break;
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

    if (ucode_xfill_define)
    {
        ucode_xfill = ucode[ucode_num];
        ucode_xfill_valid  = true;
        ucode_xfill_define = false;
        return true;
    }

    io_write_uc_placeholder(ucode_num);
    ucode_addr = UCODE_UNALLOCATED;
    ucode_hint = UCODE_UNALLOCATED;
    ++ucode_num;
    return true;
}

bool handle_ucode_raw(uint32_t addr, uint32_t uc[3])
{
    if (ucode_num >= MAXUCODE)
    {
        ERROR_LINE("too many ucodes\n");
        return false;
    }

    ucode[ucode_num] = (ucode_inst_t) {
        .addr             = addr,
        .hint             = 0,
        .cst              = 0,
        .target_addr      = 0,
        .file_name        = file_name,
        .line             = line_number,
        .uc[0]            = uc[0],
        .uc[1]            = uc[1],
        .uc[2]            = uc[2],
    };

    if (ucode_xfill_define)
    {
        ucode_xfill = ucode[ucode_num];
        ucode_xfill_valid  = true;
        ucode_xfill_define = false;
        return true;
    }

    io_write_uc_placeholder(ucode_num);
    ucode_addr = UCODE_UNALLOCATED;
    ucode_hint = UCODE_UNALLOCATED;
    ++ucode_num;
    return true;
}

bool ucode_apply_hints(void)
{
    const constraint_t *cst = 0;
    uint32_t base = 0;
    uint32_t cur = 0;

    for (uint i=last_ucode_num, j=0; i < ucode_num && j < ucode_num_hints; ++i)
    {
        // process constraint
        if (ucode[i].cst)
        {
            if (constraint_is_terminator(ucode[i].cst))
                cst = 0;
            else if (!cst)
            {
                cst = ucode[i].cst;
                // this is just to count inner/outer constraints accurately
                for (uint32_t a=region_init_addr(region_list, 0); a != REGION_FINISHED; a=region_next_addr(region_list, a))
                {
                    if (constraint_matches(cst, a))
                    {
                        cur = base = a;
                        break;
                    }
                }
                if (base == CONSTRAINT_SET_FINISHED)
                    continue; // unable to satisfy base

                if (ucode_hint_type[j] == 'B')
                    ucode[i].hint = ucode_hints[j++]; // outer
                // else (H), ignore
            }
            else
            {
                // inner-constraint may skip addrs
                if (!constraint_matches(ucode[i].cst, cur))
                {
                    cur = constraint_next(cst, ucode[i].cst, base, cur);
                    if (cur == CONSTRAINT_SET_FINISHED)
                        continue; // error
                }
            }

            continue;
        }
        else if (!cst)
        {
            if (ucode[i].addr == UCODE_UNALLOCATED)
            {
                if (ucode_hint_type[j] == 'X')
                    ++j; // skip X
                else if (ucode_hint_type[j] == 'H')
                    ucode[i].hint = ucode_hints[j++]; // outside (H)
                // else (B), ignore
            }
            continue;
        }

        if (ucode[i].addr != UCODE_UNALLOCATED)
            continue; // assigned inside (error)

        cur = constraint_next(cst, 0, base, cur);
        if (cur == CONSTRAINT_SET_FINISHED)
            cst = 0;
    }

    last_ucode_num = ucode_num;
    return true;
}

static uint32_t get_cst_base(const constraint_t *cst, uint32_t hint, uint uidx)
{

    for (uint32_t a=region_init_addr(region_list, hint); a != REGION_FINISHED; a=region_next_addr(region_list, a))
    {
        if (!constraint_matches(cst, a))
            continue;

        uint32_t base = a;
        uint32_t cur  = a;

        for (uint i=uidx+1; i <= ucode_num; ++i)
        {
            if (i == ucode_num)
                return a; // success

            // process constraint
            if (ucode[i].cst)
            {
                if (constraint_is_terminator(ucode[i].cst))
                    return a; // success
                else
                {
                    // inner-constraint may skip addrs
                    if (!constraint_matches(ucode[i].cst, cur))
                    {
                        cur = constraint_next(cst, ucode[i].cst, base, cur);
                        if (cur == CONSTRAINT_SET_FINISHED)
                            break;
                    }
                }

                continue;
            }

            if (ucode[i].addr != UCODE_UNALLOCATED)
                break; // assigned inside (error)

            if (ucode_alloc[cur])
                break; // not empty

            cur = constraint_next(cst, 0, base, cur);
            if (cur == CONSTRAINT_SET_FINISHED)
                return a; // success
        }
    }

    const char *file_name = ucode[uidx].file_name;
    uint line_number = ucode[uidx].line;
    char buf[33];
    ERROR_LINE("unable to satisfy constraint %s\n", constraint_text(buf, sizeof(buf), cst));
    return CONSTRAINT_SET_FINISHED;
}

static void init_default_ucode(uint addr, uint ucode_idx)
{
    if (ucode_xfill_valid)
    {
        ucode[ucode_idx] = ucode_xfill;
        ucode[ucode_idx].addr = addr;
        return;
    }

    uint32_t def[3] = { };

    // set defaulted fields
    for (uint i=0; i<fields.mask+1; ++i)
    {
        if (!fields.table[i].key)
            continue;

        const field_def_t *fdef = fields.table[i].value_ptr;
        if (is_set(def, fdef->li, fdef->ri))
            continue;

        if (fdef->def_flag)
            apply_val(ucode[ucode_idx].uc, fdef->li, fdef->ri, fdef->def_val);
        else if (fdef->addr_flag)
            apply_val(ucode[ucode_idx].uc, fdef->li, fdef->ri, addr);
    }
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
            ucode_inst_t *ouc = ucode_alloc[ucode[i].addr];
            if ( ouc->uc[2] == ucode[i].uc[2] &&
                 ouc->uc[1] == ucode[i].uc[1] &&
                (ouc->uc[0] & ~0x1fff) == (ucode[i].uc[0] & ~0x1fff)
            )
                ERROR_LINE_UIDX(i, "duplicate address %04x same content on line %u\n", ucode[i].addr, ouc->line);
            else
                ERROR_LINE_UIDX(i, "duplicate address %04x on line %u\n", ucode[i].addr, ouc->line);

            //return false;
            continue;
        }

        ucode_alloc[ucode[i].addr] = &ucode[i];
        ucode[i].flags.assigned = true;
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
                cst = 0;
            else if (!cst)
            {
                cst = ucode[i].cst;
                uint32_t hint = ucode[i].addr != UCODE_UNALLOCATED ? ucode[i].addr : ucode[i].hint;
                if (hint != UCODE_UNALLOCATED && !constraint_matches(cst, hint))
                    hint &= (~(cst->mmask|cst->cmask)|cst->cmask);

                cur = base = get_cst_base(cst, hint, i);
                if (base == CONSTRAINT_SET_FINISHED)
                {
                    cst = 0;
                    continue; // error
                }
                else if (hint != UCODE_UNALLOCATED && base != hint)
                {
                    WARNING("%s:%u, hint %04x failed to satisfy constraint, got %04x\n", ucode[i].file_name, ucode[i].line, hint, base);
                }
            }
            else
            {
                // inner-constraint may skip addrs
                if (!constraint_matches(ucode[i].cst, cur))
                {
                    cur = constraint_next(cst, ucode[i].cst, base, cur);
                    if (cur == CONSTRAINT_SET_FINISHED)
                    {
                        ERROR_LINE_UIDX(i, "unable to satisfy inner constraint\n");
                        continue;
                    }
                }
                ucode[i].flags.inner_cst = true;
            }

            continue;
        }
        else if (!cst)
        {
            // processed hinted ones early before other constraints fill them
            if (ucode[i].addr == UCODE_UNALLOCATED && ucode[i].hint != UCODE_UNALLOCATED)
            {
                uint32_t addr = ucode[i].hint;
                if (!ucode_alloc[addr])
                {
                    ucode_alloc[addr] = &ucode[i];
                    ucode[i].addr = addr;

                }
            }

            continue; // skip outside constraint
        }

        if (ucode[i].addr != UCODE_UNALLOCATED)
        {
            ERROR_LINE_UIDX(i, "ucode assigned address within constraint\n");
            continue;
        }

        // allocate ucode!
        ucode_alloc[cur] = &ucode[i];
        ucode[i].addr = cur;
        ucode[i].flags.constrained = true;

        cur = constraint_next(cst, 0, base, cur);
        if (cur == CONSTRAINT_SET_FINISHED)
            cst = 0;
    }

    // allocate rest
    for (uint i=0; i<ucode_num; ++i)
    {
        if (ucode[i].cst || ucode[i].addr != UCODE_UNALLOCATED)
            continue; // skip constraints and allocated

        uint32_t addr = UCODE_UNALLOCATED;
        for (uint32_t a=region_init_addr(region_list, 0); a != REGION_FINISHED; a=region_next_addr(region_list, a))
        {
            if (!ucode_alloc[a] && !ucode_xresv_seq[a])
            {
                addr = a;
                break;
            }
        }

        if (addr == UCODE_UNALLOCATED)
        {
            ERROR_LINE_UIDX(i, "can't allocate, no free address\n");
            //return false;
            continue;
        }

        if (ucode[i].hint != UCODE_UNALLOCATED && ucode_alloc[ucode[i].hint] && ucode_alloc[ucode[i].hint] != &ucode[i])
        {
            ucode_inst_t *ouc = ucode_alloc[ucode[i].hint];
            if ( ouc->uc[2] == ucode[i].uc[2] &&
                 ouc->uc[1] == ucode[i].uc[1] &&
                (ouc->uc[0] & ~0x1fff) == (ucode[i].uc[0] & ~0x1fff))
                WARNING("%s:%u, hint %04x failed same content as %s:%u\n", ucode[i].file_name, ucode[i].line, ucode[i].hint, ouc->file_name, ouc->line);
            else
                WARNING("%s:%u, hint %04x failed conflict with %s:%u\n", ucode[i].file_name, ucode[i].line, ucode[i].hint, ouc->file_name, ouc->line);
        }

        ucode_alloc[addr] = &ucode[i];
        ucode[i].addr = addr;
    }

    // fill unallocated space
    for (uint i=0; i<=MAXPC; ++i)
    {
        if (ucode_alloc[i])
            continue;

        if (ucode_num >= MAXUCODE)
        {
            ERROR("too many ucodes\n");
            return false;
        }

        init_default_ucode(i, ucode_num);
        ucode_alloc[i] = &ucode[ucode_num];

        ++ucode_num;
    }

    return true;
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
        const char *file_name = ucode[i].file_name;
        uint line_number = ucode[i].line;
        if (!ucode[i].target_addr)
            continue;

        if (strcmp(ucode[i].target_addr, this_addr_str) == 0)
            apply_val(ucode[i].uc, fdef->li, fdef->ri, ucode[i].addr);
        else if (strcmp(ucode[i].target_addr, next_addr_str) == 0)
        {
            if (i+1 >= ucode_num)
            {
                ERROR_LINE("can't use address of next instruction on last instruction\n");
                apply_val(ucode[i].uc, fdef->li, fdef->ri, ucode[i].addr); // substitute with <.>
                //return false;
                continue;
            }
            uint j;
            for (j=i+1; j<ucode_num && ucode[j].cst; ++j);
            apply_val(ucode[i].uc, fdef->li, fdef->ri, ucode[j].addr);
        }
        else
        {
            ucode_inst_t *inst = hashtable_get(&symbols, ucode[i].target_addr);

            // skip constraints after labels, label will apply to next actual ucode
            while (inst && inst->addr == UCODE_UNALLOCATED && inst->cst)
                ++inst;

            if (!inst || inst->addr == UCODE_UNALLOCATED)
            {
                ERROR_LINE("unresolved symbol %s\n", ucode[i].target_addr);
                apply_val(ucode[i].uc, fdef->li, fdef->ri, ucode[i].addr); // substitute with <.>
                //return false;
                continue;
            }
            apply_val(ucode[i].uc, fdef->li, fdef->ri, inst->addr);
        }
    }

    return true;
}

bool handle_xresv_constraint(uint32_t first, uint32_t next, const constraint_t *cst, bool resv)
{
    for (uint32_t a=region_init_addr(region_list, first); a != REGION_FINISHED && a < next; a=region_next_addr(region_list, a))
    {
        if (constraint_matches(cst, a))
        {
            uint32_t cur = a;
            while (cur != CONSTRAINT_SET_FINISHED)
            {
                ucode_xresv_cst[cur] = resv;
                cur = constraint_next(cst, 0, a, cur);
            }
        }
    }
    return true;
}

bool handle_xresv_sequential(uint32_t first, uint32_t next, bool resv)
{
    for (uint32_t a=region_init_addr(region_list, first); a != REGION_FINISHED && a < next; a=region_next_addr(region_list, a))
    {
        ucode_xresv_seq[a] = resv;
    }
    return true;
}

bool handle_xhint(uint32_t hint)
{
    ucode_hint = hint;
    return true;
}

bool handle_xfill(void)
{
    ucode_xfill_define = true;
    return true;
}

