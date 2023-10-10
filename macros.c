// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "macros.h"

#include "hashtable.h"
#include "utils.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

hashtable_t macros;

bool macros_init(void)
{
    return hashtable_init(&macros, 256);
}

bool handle_macro_def(const char *macro, const char *value)
{
    char *p = strdup(value);

    // strip leading "
    if (p[0] == '"')
        ++p;

    // strip trailing "
    uint len = strlen(p);
    if (len > 0 && p[len-1] == '"')
        p[len-1] = 0;

    if (!hashtable_add(&macros, macro, p))
    {
        ERROR("macro %s already defined\n", macro);
        return false;
    }

    return true;
}

const char *macro_get(const char *macro)
{
    return hashtable_get(&macros, macro);
}
