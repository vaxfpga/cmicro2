// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_UCODE_H)
#define INCLUDED_UCODE_H

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"


#define UCODE_UNALLOCATED (~(uint32_t)0)

typedef struct constraint_s constraint_t;

typedef struct ucode_inst_s
{
    uint32_t addr;
    const constraint_t *cst;
    const char *target_addr;
    uint line;
    uint32_t uc[3];
} ucode_inst_t;

typedef struct ucode_field_s
{
    const char *name;
    const char *valstr;
    uint32_t    valint;
} ucode_field_t;

extern ucode_inst_t ucode[MAXUCODE];
extern uint ucode_num;

extern ucode_inst_t *ucode_alloc[MAXPC+1];

// callbacks to be defined by impl
extern bool io_write_uc_placeholder(uint ucode_idx);

bool ucode_init(void);

bool ucode_allocate(void);
bool ucode_resolve(void);

bool handle_region(uint32_t low, uint32_t high);
bool handle_constraint(const constraint_t *cst);
bool handle_addr (uint32_t addr);
bool handle_label(const char *label);
bool handle_ucode(const ucode_field_t *field, uint num);

bool handle_xresv_constraint(uint32_t first, uint32_t next, const constraint_t *cst, bool resv);
bool handle_xresv_sequential(uint32_t first, uint32_t next, bool resv);

#endif // INCLUDED_UCODE_H
