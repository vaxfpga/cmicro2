// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#if !defined(INCLUDED_PARSER_H)
#define INCLUDED_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "utils.h"


// forward decls
typedef struct constraint_s  constraint_t;
typedef struct field_def_s   field_def_t;
typedef struct region_list_s region_list_t;
typedef struct ucode_field_s ucode_field_t;


// callbacks to be defined by impl
extern bool io_get_line(char *buf, uint max);
extern bool io_write_expanded(const char *xline);

//extern bool handle_directive (const char *directive, const char *value);
extern bool handle_region    (region_list_t *r);
extern bool handle_field_def (const char *field,     const field_def_t *fdef);
extern bool handle_field_val (const char *field_val, uint32_t value);
extern bool handle_constraint(const constraint_t *cst);
extern bool handle_macro_def (const char *macro,     const char *value);
extern bool handle_addr      (uint32_t addr);
extern bool handle_label     (const char *label);
extern bool handle_ucode     (const ucode_field_t *field, uint numf);
extern bool handle_ucode_raw (uint32_t addr, uint32_t uc[3]);

extern bool handle_xresv_constraint(uint32_t first, uint32_t next, const constraint_t *cst, bool resv);
extern bool handle_xresv_sequential(uint32_t first, uint32_t next, bool resv);

extern bool handle_xhint(uint32_t hint);
extern bool handle_xfill(void);

// normalizes, removes comments, and splices
bool get_logical_line(char *line, uint max);

// parses logical lines
bool parse_directive(const char *name, const char *str);
const char *parse_constraint(const char *str, constraint_t *cst);
bool parse_field_def(const char *name, const char *str);
bool expand_line(char *xline, uint max, const char *line);
bool parse_microcode(const char *line);
bool parse_ucode_text(const char *str);

bool parse_line(const char *line);
bool parse_end_of_file(void);
bool parse_hints_line(const char *line);


#endif // INCLUDED_PARSER_H
