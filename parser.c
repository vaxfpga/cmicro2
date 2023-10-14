// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

#include "parser.h"

#include "constraints.h"
#include "fields.h"
#include "macros.h"
#include "ucode.h"
#include "utils.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct args_s
{
    char buf[MAXLINE];
    char *arg[MAXARGS];
    uint nargs;
} args_t;

static inline const char *skip_ws(const char *str)
{
    while (isspace(*str))
        ++str;
    return str;
}

static inline char *remove_trailing_ws(char *buffer, char *eos)
{
    if (eos <= buffer)
        return buffer;
    while (*--eos == ' ')
        *eos = 0;
    return eos+1;
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

        // convert any kind of spaces to single ' '
        if (isspace(*p))
            *p = ' ';

        // uppercase everything
        *p = toupper(*p);

        ++p;
    }

    remove_trailing_ws(line, p);
}

//static inline char *safe_strncpy(char *dst, const char *src, size_t n)
//{
//    if (n > 0)
//    {
//        strncpy(dst, src, len);
//        dst[n-1] = 0;
//    }
//    return dst;
//}

static inline bool is_token(char c)
{
    return (c == ' ') || isalnum(c) || (c != 0 && strchr("!#&()<>*+-.?_", c) != 0);
}

static const char *parse_token(const char *str, char *name, uint max, args_t *args, bool allow_bracket)
{
    char *p = name;
    char *q = args ? args->buf : 0;
    uint maxa = sizeof(args->buf);
    uint numa = 0;

    if (max <= 1)
        return 0;

    if (args)
    {
        args->buf[0] = 0;
        args->arg[0] = args->buf;
        args->nargs = 0;
    }

    uint bracket = 0;
    while (max > 1 && maxa > 1 && numa < MAXARGS)
    {
        if (!is_token(*str))
        {
            if (!allow_bracket)
                break;
            else if (*str == ',' || *str == ']')
            {
                if (bracket <= 0)
                    break;
            }
            else if (*str != '[')
                break;
        }

        if (bracket > 0 && (*str == ']' || *str == ','))
        {
            --bracket;

            if (bracket <= 0 && q) // end of arg
            {
                *q = 0;
                q = remove_trailing_ws(args->arg[numa], q);
                args->arg[++numa] = ++q;
                maxa = (args->buf+sizeof(args->buf) - q);
            }
        }

        if (bracket <= 0)
        {
            *p++ = *str, --max;
        }
        else if (q)
            *q++ = *str, --maxa;

        if (*str == '[' || *str == ',')
            ++bracket;

        // normalize internal spaces to single space
        if (isspace(*str))
            str = skip_ws(str);
        else
            ++str;
    }

    *p = 0;
    remove_trailing_ws(name, p);

    if (q && numa < MAXARGS)
    {
        *q = 0;
        for (uint i=numa; i<MAXARGS; ++i)
            args->arg[i] = 0;
        args->nargs = numa;
    }

    if (max <= 1 || maxa <= 1)
        return 0;

    return str;
}

bool get_logical_line(char *line, uint max)
{
    char *p = line;
    *p = 0;

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

bool parse_directive(const char *line, const char *name, const char *str)
{
    DEBUG("parsing directive: %s\n", line);

    const char *p = skip_ws(str);

    if (strcmp(name, ".REGION") == 0)
    {
        // TODO: should allow multiple ranges...
        if (*p != '/')
        {
            ERROR("bad region directive: %s\n", line);
            return false;
        }

        p = skip_ws(++p);

        char *q = 0;
        uint32_t low = strtoul(p, &q, 16);
        if (q == p) // couldn't parse any numbers
        {
            ERROR("bad region directive: %s\n", line);
            return false;
        }

        p = skip_ws(q);
        if (*p != ',')
        {
            ERROR("bad region directive: %s\n", line);
            return false;
        }

        p = skip_ws(++p);

        q = 0;
        uint32_t high = strtoul(p, &q, 16);
        if (q == p) // couldn't parse any numbers
        {
            ERROR("bad region directive: %s\n", line);
            return false;
        }

        DEBUG("parsed region directive: 0x%04x, 0x%04x\n", low, high);
        return handle_region(low, high);
    }

    DEBUG("parsed directive: %s %s\n", name, str);

    if (!handle_directive(name, str))
        return false;

    return true;
}

bool parse_constraint(const char *str)
{
    DEBUG("parsing constraint: %s\n", str);

    constraint_t cst = { };

    const char *p = str;

    if (*p != '=')
        return false;

    p = skip_ws(++p);

    while (*p == '0' || *p == '1' || *p == '*')
    {
        cst.incr  <<= 1;
        cst.mmask <<= 1;
        cst.cmask <<= 1;

        if (*p == '0')
        {
            cst.incr = 1;
            cst.mmask |= 1;
        }
        else if (*p == '1')
        {
            cst.mmask |= 1;
            cst.cmask |= 1;
        }
        else if (*p == '*')
        {
            cst.cmask |= 1;
        }

        ++p;
    }

    cst.vmask = cst.mmask & cst.cmask;

    DEBUG("parsed constraint: i=%08b v=%08b m=%08b c=%08b\n",
          cst.incr, cst.vmask, cst.mmask, cst.cmask);

    if (!handle_constraint(&cst))
        return false;

    return true;
}

bool parse_field_def(const char *name, const char *str)
{
    DEBUG("parsing field def: %s /= %s\n", name, str);

    field_def_t fdef = { };
    const char *p = skip_ws(str);

    if (*p != '<')
        return false;
    ++p;

    char *q = 0;
    fdef.li = fdef.ri = strtoul(p, &q, 10);
    if (q == p) // couldn't parse any numbers
        return false;
    p = skip_ws(q);

    if (*p == ':')
    {
        ++p;
        fdef.ri = strtoul(p, &q, 10);
        if (q == p) // couldn't parse any numbers
            return false;
        p = skip_ws(q);
    }

    if (*p == '>')
    {
        ++p;
        p = skip_ws(p);
    }
    else
        return false;

    if (*p == ',')
    {
        ++p;
        p = skip_ws(p);

        char name[32];
        p = parse_token(p, name, sizeof(name), 0, false);

        if (strcmp(name, ".DEFAULT") == 0)
        {
            p = skip_ws(p);
            if (*p != '=')
                return false;
            ++p;

            fdef.def_val = strtoul(p, &q, 16);
            if (q == p)  // couldn't parse any numbers
                return false;

            p = skip_ws(q);
            if (*p != 0)
                return false;

            fdef.def_flag  = true;
            fdef.addr_flag = false;
            fdef.next_flag = false;
        }
        else if (strcmp(name, ".ADDRESS") == 0)
        {
            fdef.def_flag  = false;
            fdef.addr_flag = true;
            fdef.next_flag = false;
        }
        else if (strcmp(name, ".NEXTADDRESS") == 0)
        {
            fdef.def_flag  = false;
            fdef.addr_flag = true;
            fdef.next_flag = true;
        }
        else
            return false;
    }

    if (*p != 0)
        return false;

    DEBUG("parsed field def: li=%u, ri=%u def=0x%0x flags=0b%03b\n",
            fdef.li, fdef.ri, fdef.def_val, (fdef.def_flag<<2|fdef.addr_flag|fdef.next_flag));

    if (!handle_field_def(name, &fdef))
        return false;

    return true;
}

static char *expand_macro(char *xline, uint max, const char *macro_name, const args_t *args)
{
    const char *p = macro_get(macro_name);
    char *q = xline;

    if (!p)
    {
        uint len = strlen(macro_name);
        if (len+1 > max)
            return 0;
        strncpy(q, macro_name, max);
        q += len;
        max -= len;
        q = remove_trailing_ws(xline, q);
        return q;
    }

    while (max > 1 && *p)
    {
        if (*p == '@')
        {
            uint ai = (*++p)-'1';
            if (ai > MAXARGS)
                return 0;
            const char *a = args->arg[ai];
            int len = strlen(a);
            if (len+1 > max)
                return false;
            strncpy(q, a, max);
            q += len;
            max -= len;
            ++p;
        }
        else
            *q++ = *p++, --max;
    }

    *q = 0;
    q = remove_trailing_ws(xline, q);

#if defined(ENABLE_DEBUG)
    DEBUG("expanding macro %s%s", macro_name, args->nargs > 0 ? "(" : "");
    if (args->nargs > 0)
    {
        for (uint di=0; di<args->nargs; ++di)
            fprintf(stderr, "%s%s", args->arg[di], (di+1)<args->nargs ? "," : ")");
    }
    fprintf(stderr, " to \"%s\"\n", xline);
#endif

    return q;
}

static bool expand_line_once(char *xline, uint max, const char *line)
{
    char name[MAXLINE]; // could be entire line
    args_t args;

    const char *p = line;
    char *q = xline;
    uint xmax = max;
    while(*p && max > 1)
    {
        p = parse_token(p, name, sizeof(name), &args, true);
        if (!p)
            return false;

        q = expand_macro(q, max, name, &args);
        if (!q)
            return false;
        max = xline+xmax-q;

        if (max <= 1)
            return false;

        if (*p == ',' || *p == '/')
        {
            *q++ = *p++, --max;
            p = skip_ws(p);
        }
        else if (*p != 0)
            return false;
    }

    *q = 0;
    remove_trailing_ws(xline, q);

    return true;
}

bool expand_line(char *xline, uint max, const char *line)
{
    DEBUG("expanding macros for line: \"%s\"\n", line);
    char tmp[MAXLINE];
    for (uint i=0; i<MAXRECURSE; ++i)
    {
        strncpy(tmp, line, sizeof(tmp));
        if (tmp[sizeof(tmp)-1] != 0)
            return false;

        if (!expand_line_once(xline, max, tmp))
            return false;

        if (strcmp(xline, tmp) == 0)
        {
            DEBUG("expanded line: \"%s\"\n", xline);
            return true;
        }

        line = xline;
    }
    return false;
}

bool parse_microcode(const char *line)
{
    DEBUG("parsing microcode: %s\n", line);

    char xline[MAXLINE];
    const char *p = xline;
    expand_line(xline, sizeof(xline), line);

    char buf[MAXLINE];
    char *q = buf;
    uint max = sizeof(buf);

    ucode_field_t field[MAXFIELD] = { };

    uint i;
    for (i = 0; i < MAXFIELD; ++i)
    {
        char name[MAXNAME];

        p = parse_token(p, name, sizeof(name), 0, false);
        if (!p)
            return false;

        uint len = strlen(name)+1;
        if (len > max)
            return false;

        strncpy(q, name, max);
        field[i].name = q;
        q += len;

        if (*p != '/')
            return false;

        p = skip_ws(++p);

        p = parse_token(p, name, sizeof(name), 0, false);
        if (!p)
            return false;

        len = strlen(name)+1;
        if (len > max)
            return false;

        if (isdigit(name[0]))
        {
            char *r = 0;
            field[i].valstr = 0;
            field[i].valint = strtoul(name, &r, 16);
            if (r == name) // couldn't parse any numbers
                return false;
        }
        else
        {
            strncpy(q, name, max);
            field[i].valstr = q;
            field[i].valint = 0;
        }
        q += len;

        if (!*p)
        {
            if(!handle_ucode(field, i+1))
                return false;

            return true;
        }
        else if (*p != ',')
            return false;

        p = skip_ws(++p);
    }

    return false;
}

bool parse_line(const char *line)
{
    const char *p = skip_ws(line);

    char name[MAXNAME];
    p = parse_token(p, name, sizeof(name), 0, true);
    if (!p)
        return false;

    if (name[0] == '.') // directive
    {
        if (!parse_directive(line, name, p))
            return false;
    }
    else if (*p == '/' && p[1] == '=') // field def
    {
        p += 2; // '/='

        if (!parse_field_def(name, p))
            return false;
    }
    else if (*p == '=')
    {
        if (name[0] != 0) // field val
        {
            DEBUG("parsing field val: %s %s\n", name, p);
            ++p;
            char *q = 0;
            uint32_t val = strtoul(p, &q, 16);
            if (q == p) // couldn't parse any numbers
                return false;

            p = skip_ws(q);
            if (*p)
                return false;

            DEBUG("parsed field val: %s 0x%x\n", name, val);

            if (!handle_field_val(name, val))
                return false;
        }
        else // constraint
        {
            if (!parse_constraint(p))
                return false;
        }
    }
    else if (*p == '"') // macro
    {
        DEBUG("parsing macro: %s\n", line);

        // check trailing '"'
        const char *q = &p[strlen(p)-1];
        if (*q != '"')
            return false;

        if (!handle_macro_def(name, p))
            return false;

        DEBUG("parsed macro: %s %s\n", name, p);
    }
    else if (*p == ':') // addr or label
    {
        DEBUG("parsing address/label: %s:\n", name);

        // if starts with a digit (0-9) it is an address, not label
        if (isdigit(name[0]))
        {
            char *q = 0;
            uint32_t addr = strtoul(name, &q, 16);
            if (q == name) // couldn't parse any numbers
                return false;

            if (!handle_addr(addr))
                return false;

            DEBUG("parsed address 0x%0x\n", addr);
        }
        else
        {
            if (!handle_label(name))
                return false;

            DEBUG("parsed label %s\n", name);
        }

        // microcode can follow address/label on same line
        p = skip_ws(p+1);
        if (*p)
        {
            if (!parse_microcode(p))
                return false;
        }
    }
    else  // microcode
    {
        if (!parse_microcode(line))
            return false;
    }

    return true;
}
