// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_MACROS_H)
#define INCLUDED_MACROS_H

#include <stdbool.h>


bool macros_init(void);
bool handle_macro_def(const char *macro, const char *value);
const char *macro_get(const char *macro);

#endif // INCLUDED_MACROS_H
