// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_FIELDS_H)
#define INCLUDED_FIELDS_H

#include "hashtable.h"
#include "utils.h"

#include <stdbool.h>
#include <stdint.h>

#define FIELD_RANGE_INVALID (~(uint32_t)0)

typedef struct field_def_s
{
    uint li;
    uint ri;

    uint32_t def_val;

    bool def_flag;
    bool addr_flag;
    bool next_flag;

    hashtable_t vals;
} field_def_t;

extern hashtable_t fields;

bool fields_init(void);

static const field_def_t *field_def_get(const char *fname)
{
    return hashtable_get(&fields, fname);
}

static uint32_t field_val_get(const field_def_t *fdef, const char *fval)
{
    return hashtable_geti(&fdef->vals, fval);
}

bool handle_field_def(const char *field, const field_def_t *fdef);
bool handle_field_val(const char *field_val, uint32_t value);

#endif // INCLUDED_FIELDS_H
