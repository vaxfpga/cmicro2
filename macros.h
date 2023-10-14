// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_MACROS_H)
#define INCLUDED_MACROS_H

#include "hashtable.h"

#include <stdbool.h>


extern hashtable_t macros;


bool macros_init(void);

static const char *macro_get(const char *macro)
{
    return hashtable_get(&macros, macro);
}

bool handle_macro_def(const char *macro, const char *value);

#endif // INCLUDED_MACROS_H
