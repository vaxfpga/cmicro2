// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_PARSER_H)
#define INCLUDED_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"


// forward decls
typedef struct field_def_s field_def_t;
typedef struct constraint_s constraint_t;

// callbacks to be defined by impl
extern bool io_get_line(char *buf, uint max);

extern bool handle_directive (const char *directive, const char *value);
extern bool handle_field_def (const char *field,     const field_def_t *fdef);
extern bool handle_field_val (const char *field_val, uint32_t value);
extern bool handle_macro_def (const char *macro,     const char *value);
extern bool handle_microcode (const char *microcode);
extern bool handle_constraint(const constraint_t *cst);
extern bool handle_addr      (uint32_t addr);
extern bool handle_label     (const char *label);

bool parse_field_def(field_def_t *fdef, const char *str);
bool parse_constraint(constraint_t *cst, const char *str);

// normalizes, removes comments, and splices
bool get_logical_line(char *line, uint max);

// parses logical lines
bool parse_line(char *line);

#endif // INCLUDED_PARSER_H
