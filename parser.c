// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "parser.h"

#include "constraints.h"
#include "fields.h"
#include "utils.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static inline const char *skip_ws(const char *str)
{
    while (isspace(*str))
        ++str;
    return str;
}

static inline void remove_trailing_ws(char *buffer, char *eos)
{
    if (eos <= buffer)
        return;
    while (*--eos == ' ')
        *eos = 0;
}

static void normalize_line(char *line)
{
    char *p = line;
    while (*p)
    {
        // remove comment from ";" to end of line
        if (*p == ';')
        {
            *p = 0;
            break;
        }

        // convert any kind of spaces to ' '
        if (isspace(*p))
            *p = ' ';

        // uppercase everything
        *p = toupper(*p);

        ++p;
    }

    remove_trailing_ws(line, p);
}

static inline bool is_name_or_num(char c)
{
    return (c == ' ') || isalnum(c) || (strchr("!#&()*+-.?_<>", c) != 0);
}

static const char *parse_name_or_num(const char *str, char *name, uint max, bool allow_bracket)
{
    char *p = name;

    if (max <= 1)
        return str;

    bool bracket = false;
    while (max > 1)
    {
        if (!is_name_or_num(str[0]))
        {
            if (allow_bracket && !bracket && str[0] == '[')
                bracket = true;
            else if (bracket && str[0] == ']')
                bracket = false;
            else if (!bracket || str[0] != ',')
                break;
        }

        p[0] = str[0];
        ++str;
        ++p;
        --max;
    }
    p[0] = 0;

    remove_trailing_ws(name, p);
    return str;
}

bool get_logical_line(char *line, uint max)
{
    char *p = line;
    p[0] = 0;

    while (true)
    {
        if (!io_get_line(p, max))
            return false;

        normalize_line(p);

        uint len = strlen(p);

        // if blank or last char is ',', join physical lines
        // otherwise break to return
        if (len > 0 && p[len - 1] != ',')
            break;

        p   += len;
        max -= len;
    }

    DEBUG("read line len %u: |%s|\n", strlen(line), line);
    return true;
}


bool parse_constraint(constraint_t *cst, const char *str)
{
    DEBUG("parsing constraint: %s\n", str);

    *cst = (constraint_t) { };
    const char *p = str;

    if (p[0] != '=')
        return false;
    ++p;

    p = skip_ws(p);

    while (p[0] == '0' || p[0] == '1' || p[0] == '*')
    {
        cst->incr  <<= 1;
        cst->mmask <<= 1;
        cst->cmask <<= 1;

        if (p[0] == '0')
        {
            cst->incr = 1;
            cst->mmask |= 1;
        }
        else if (p[0] == '1')
        {
            cst->mmask |= 1;
            cst->cmask |= 1;
        }
        else if (p[0] == '*')
        {
            cst->cmask |= 1;
        }

        ++p;
    }

    cst->vmask = cst->mmask & cst->cmask;

    DEBUG("parsed constraint: i=%08b v=%08b m=%08b c=%08b\n",
          cst->incr, cst->vmask, cst->mmask, cst->cmask);

    return (cst->mmask != 0 || cst->cmask != 0);
}

bool parse_field_def(field_def_t *fdef, const char *str)
{
    DEBUG("parsing field def: %s\n", str);

    const char *p = skip_ws(str);

    if (p[0] != '<')
        return false;
    ++p;

    char *q = 0;
    fdef->li = strtoul(p, &q, 10);
    if (q == p)
        return false;
    p = skip_ws(q);

    if (p[0] == ',')
    {
        ++p;

        fdef->ri = strtoul(p, &q, 10);
        if (q == p)
            return false;
        p = skip_ws(q);
    }

    if (p[0] == '>')
    {
        ++p;

        fdef->ri = fdef->li;
        p = skip_ws(p);
    }
    else
        return false;

    if (p[0] == ',')
    {
        ++p;
        p = skip_ws(p);

        char name[32];
        p = parse_name_or_num(p, name, sizeof(name), false);

        if (strcmp(name, ".DEFAULT") == 0)
        {
            p = skip_ws(p);
            if (p[0] != '=')
                return false;
            ++p;

            fdef->def_val = strtoul(p, &q, 16);
            if (q == p)
                return false;

            p = skip_ws(q);
            if (p[0] != 0)
                return false;

            fdef->def_flag  = true;
            fdef->addr_flag = false;
            fdef->next_flag = false;
        }
        else if (strcmp(name, ".ADDRESS") == 0)
        {
            fdef->def_flag  = false;
            fdef->addr_flag = true;
            fdef->next_flag = false;
        }
        else if (strcmp(name, ".NEXTADDRESS") == 0)
        {
            fdef->def_flag  = false;
            fdef->addr_flag = true;
            fdef->next_flag = true;
        }
        else
            return false;
    }

    if (p[0] != 0)
        return false;

    DEBUG("parsed field def: li=%u, ri=%u def=0x%02x flags=0b%03b\n",
            fdef->li, fdef->ri, fdef->def_val, (fdef->def_flag<<2|fdef->addr_flag|fdef->next_flag));

    return true;
}

bool parse_line(char *line)
{
    const char *p = skip_ws(line);

    char name[128];
    p = parse_name_or_num(p, name, sizeof(name), true);

    if (name[0] == '.') // directive
    {
        DEBUG("parsing directive: %s\n", line);
        DEBUG("parsed directive: %s %s\n", name, p);

        if (!handle_directive(name, p))
            return false;
    }
    else if (p[0] == '/' && p[1] == '=') // field def
    {
        p += 2; // '/='

        field_def_t fdef;
        if (!parse_field_def(&fdef, p))
            return false;

        if (!handle_field_def(name, &fdef))
            return false;
    }
    else if (p[0] == '=')
    {
        if (name[0] != 0) // field val
        {
            DEBUG("parsing field val: %s %s\n", name, p);
            ++p;
            char *q = 0;
            uint32_t val = strtoul(p, &q, 16);
            if (q == p)
                return false;

            p = skip_ws(q);
            if (p[0] != 0)
                return false;

            DEBUG("parsed field val: %s %x\n", name, val);

            if (!handle_field_val(name, val))
                return false;
        }
        else // constraint
        {
            constraint_t cst;
            if (!parse_constraint(&cst, p))
                return false;
            //handle_constraint
            //DEBUG("parsed constraint: %s\n", p);
        }
    }
    else if (p[0] == '"') // macro
    {
        DEBUG("parsing macro: %s\n", line);
        ++p; // skip leading '"'

        // remove trailing '"'
        char *q = &line[strlen(line)-1];
        if (q[0] == '"')
            q[0] = 0;

        if (!handle_macro_def(name, p))
            return false;

        DEBUG("parsed macro: %s %s\n", name, p);
    }
    else if (p[0] == ':') // addr or label
    {
        DEBUG("parsing address/label: %s:\n", name);

        char *q = 0;
        uint32_t addr = strtoul(name, &q, 16);

        // if starts with a digit (0-9) and all are (hex) digits, go with address
        if (isdigit(name[0]) && *q == 0)
        {
            DEBUG("parsed address 0x%0x\n", addr);
        }
        else
        {
            DEBUG("parsed label %s\n", name);
        }
    }
    else  // microcode
    {
        DEBUG("parsing microcode: %s\n", line);
    }
}
