// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "macros.h"

#include "utils.h"
#include "hashtable.h"

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
    if (!hashtable_add(&macros, macro, strdup(value)))
        return false;

    return true;
}

bool expand_macros(char *line, uint max, const char *orig)
{

}
