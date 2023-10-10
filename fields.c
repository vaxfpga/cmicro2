// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "fields.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

hashtable_t fields;

field_def_t *field_active = 0;
const char *field_active_name = 0;

bool fields_init(void)
{
    return hashtable_init(&fields, 256);
}

bool handle_field_def(const char *field, const field_def_t *fdef)
{
    field_def_t *f = calloc(1, sizeof(field_def_t));
    if (!f)
        return false;

    *f = *fdef;

    if (!hashtable_init(&f->vals, 16))
        return false;

    if (!hashtable_add(&fields, field, f))
    {
        ERROR("field %s already defined\n", field);
        return false;
    }

    field_active = f;
    field_active_name = field;
    return true;
}

bool handle_field_val(const char *field_val, uint32_t value)
{
    if (!field_active || !field_active_name)
        return false;

    if (!hashtable_addi(&field_active->vals, field_val, value))
    {
        ERROR("field value %s already defined in field %s\n", field_val, field_active_name);
        return false;
    }

    return true;
}
