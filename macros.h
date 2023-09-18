// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_MACROS_H)
#define INCLUDED_MACROS_H

#include "utils.h"
#include "hashtable.h"

#include <stdbool.h>
#include <stdint.h>

#define FIELD_RANGE_INVALID (~(uint32_t)0)

//typedef struct field_def_s
//{
//    uint li;
//    uint ri;
//
//    uint32_t def_val;
//
//    bool def_flag;
//    bool addr_flag;
//    bool next_flag;
//
//    hashtable_t vals;
//} field_def_t;
//
//bool fields_init(void);
//
//bool handle_field_def (const char *field, const field_def_t *fdef);
//bool handle_field_val (const char *field_val, uint32_t value);
//
//extern hashtable_t fields;

bool macros_init(void);
bool handle_macro_def(const char *macro, const char *value);

#endif // INCLUDED_MACROS_H
