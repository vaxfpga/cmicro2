// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "fields.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

hashtable_t fields;

field_def_t *field_active = 0;
char field_active_name[MAXNAME];

bool fields_init(void)
{
    return hashtable_init(&fields, 256);
}

bool handle_field_def(const char *field, const field_def_t *fdef)
{
    if (!field || !fdef)
    {
        field_active = 0;
        return false;
    }

    if (fdef->li < fdef->ri)
    {
        ERROR_LINE("illegal field indexes <%u:%u> in %s\n", fdef->li, fdef->ri, field);
        return false;
    }

    field_def_t *f = calloc(1, sizeof(field_def_t));
    if (!f)
    {
        ERROR_LINE("allocation failed (fdef) %s\n", field);
        return false;
    }

    *f = *fdef;

    if (!hashtable_init(&f->vals, 16))
    {
        ERROR_LINE("hashtable fail (fdef) %s\n", field);
        return false;
    }

    if (!hashtable_add(&fields, field, f))
    {
        ERROR_LINE("field %s already defined\n", field);
        return false;
    }

    field_active = f;
    if (strlen(field)+1 > sizeof(field_active_name))
    {
        ERROR_LINE("field name buffer overflow on %s\n", field);
        return false;
    }

    strncpy(field_active_name, field, sizeof(field_active_name));
    return true;
}

bool handle_field_val(const char *field_val, uint32_t value)
{
    if (!field_active)
    {
        ERROR_LINE("field value %s = 0x%x cannot be defined outside active field\n", field_val, value);
        return false;
    }

    if (!hashtable_addi(&field_active->vals, field_val, value))
    {
        ERROR_LINE("field value %s = 0x%x already defined in field %s\n", field_val, value, field_active_name);
        return false;
    }

    return true;
}
