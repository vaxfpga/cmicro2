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

    DEBUG_LINES("normalized: %s\n", line);
    return true;
}

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
    while (max > 1 && *str && maxa > 1 && numa < MAXARGS)
    {
        bool escape = false;
        if (!is_token(*str))
        {
            if (!allow_bracket)
                break;
            else if (*str == '@')
            {
                escape = true;
                ++str;
                if (!*str)
                    break;
            }
            else if (bracket <= 0 && *str != '[')
                break;
        }

        if (bracket > 0 && (*str == ']' || *str == ',') && !escape)
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
            *p++ = *str, --max;
        else if (q)
            *q++ = *str, --maxa;

        if ((*str == '[' || *str == ',') && !escape)
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

    if (max <= 1)
    {
        ERROR_LINE("token name truncated: %s\n", name);
        return 0;
    }

    if (maxa <= 1)
    {
        ERROR_LINE("argument list overflow\n");
        return 0;
    }

    return str;
}

bool parse_directive(const char *name, const char *str)
{
    DEBUG_PARSING("parsing directive: %s %s\n", name, str);

    const char *p = skip_ws(str);

    if (strcmp(name, ".REGION") == 0)
    {
        // TODO: should allow multiple ranges...
        if (*p != '/')
        {
            ERROR_LINE("region syntax in %s %s: '/' expected after .REGION\n", name, str);
            return false;
        }

        p = skip_ws(p+1);

        char *q = 0;
        uint32_t low = strtoul(p, &q, 16);
        if (q == p) // couldn't parse any numbers
        {
            ERROR_LINE("region syntax in %s %s: number expected after '/'\n", name, str);
            return false;
        }

        p = skip_ws(q);
        if (*p != ',')
        {
            ERROR_LINE("region syntax in %s %s: ',' expected after low address\n", name, str);
            return false;
        }

        p = skip_ws(p+1);

        q = 0;
        uint32_t high = strtoul(p, &q, 16);
        if (q == p) // couldn't parse any numbers
        {
            ERROR_LINE("region syntax in %s %s: number expected after ','\n", name, str);
            return false;
        }

        p = skip_ws(q);
        if (*p)
        {
            ERROR_LINE("region syntax in %s %s: bad char %c after high address\n", name, str, *p);
            return false;
        }

        DEBUG_PARSING("parsed region directive: 0x%04x, 0x%04x\n", low, high);
        return handle_region(low, high);
    }
    else if (strcmp(name, ".XRESERVE") == 0 || strcmp(name, ".XUNRESERVE") == 0)
    {
        bool resv = (name[2] == 'R');

        if (*p != '/')
        {
            ERROR_LINE("xreserve syntax in %s %s: '/' expected after .XREGION\n", name, str);
            return false;
        }

        p = skip_ws(p+1); // '/'

        char *q = 0;
        uint32_t first = strtoul(p, &q, 16);
        uint32_t next = first+1;
        if (q == p) // couldn't parse any numbers
        {
            ERROR_LINE("xreserve syntax in %s %s: number expected after .XREGION\n", name, str);
            return false;
        }

        p = skip_ws(q);

        if (*p == ',')
        {
            p = skip_ws(p+1); // ','

            q = 0;
            next = strtoul(p, &q, 16);
            if (q == p) // couldn't parse any numbers
            {
                ERROR_LINE("xreserve syntax in %s %s: number expected after .XREGION\n", name, str);
                return false;
            }
        }

        p = skip_ws(q);

        if (*p == '=')
        {
            p = skip_ws(p+1); // '='

            constraint_t cst = { };
            p = parse_constraint(p, &cst);
            if (!p)
                return false;

            if (!handle_xresv_constraint(first, next, &cst, resv))
                return false;

            DEBUG_PARSING("parsed xreserve constraint directive\n");
        }
        else
        {
            if (!handle_xresv_sequential(first, next, resv))
                return false;

            DEBUG_PARSING("parsed xreserve sequential directive\n");
        }
    }
    else if (strcmp(name, ".XHINT") == 0)
    {
        if (*p != '/')
        {
            ERROR_LINE("xreserve syntax in %s %s: '/' expected after .XHINT\n", name, str);
            return false;
        }

        p = skip_ws(p+1); // '/'

        char *q = 0;
        uint32_t hint = strtoul(p, &q, 16);
        if (q == p) // couldn't parse any numbers
        {
            ERROR_LINE("xhint syntax in %s %s: number expected after .XHINT\n", name, str);
            return false;
        }

        if (!handle_xhint(hint))
            return false;

        DEBUG_PARSING("parsed xhint directive\n");
    }

    DEBUG_PARSING("parsed directive: %s %s\n", name, str);

    return true;
}

const char *parse_constraint(const char *str, constraint_t *cst)
{
    DEBUG_PARSING("parsing constraint: %s\n", str);

    const char *p = skip_ws(str);

    if (*p && *p != '0' && *p != '1' && *p != '*')
    {
        ERROR_LINE("constraint syntax in =%s: bad char %c after '='\n", str, *p);
        return 0;
    }

    while (*p == '0' || *p == '1' || *p == '*')
    {
        cst->incr  <<= 1;
        cst->mmask <<= 1;
        cst->cmask <<= 1;

        if (*p == '0')
        {
            cst->incr = 1;
            cst->mmask |= 1;
        }
        else if (*p == '1')
        {
            cst->mmask |= 1;
            cst->cmask |= 1;
        }
        else if (*p == '*')
        {
            cst->cmask |= 1;
        }

        ++p;
    }

    if (*p && !isspace(*p))
    {
        ERROR_LINE("constraint syntax in =%s: bad char %c after constraint\n", str, *p);
        return false;
    }

    cst->vmask = cst->mmask & cst->cmask;

    DEBUG_PARSING("parsed constraint: i=%08b v=%08b m=%08b c=%08b\n",
          cst->incr, cst->vmask, cst->mmask, cst->cmask);


    return p;
}

bool parse_field_def(const char *name, const char *str)
{
    DEBUG_PARSING("parsing field def: %s /= %s\n", name, str);

    if (!name[0])
    {
        ERROR_LINE("field def syntax in /= %s: empty name\n", str);
        return false;
    }

    field_def_t fdef = { };
    const char *p = skip_ws(str);

    if (*p != '<')
    {
        ERROR_LINE("field def syntax in %s /= %s: '<' expected after '/='\n", name, str);
        return false;
    }
    ++p;

    char *q = 0;
    fdef.li = fdef.ri = strtoul(p, &q, 10);
    if (q == p) // couldn't parse any numbers
    {
        ERROR_LINE("field def syntax in %s /= %s: number expected after '<'\n", name, str);
        return false;
    }
    p = skip_ws(q);

    if (*p == ':')
    {
        ++p;
        fdef.ri = strtoul(p, &q, 10);
        if (q == p) // couldn't parse any numbers
        {
            ERROR_LINE("field def syntax in %s /= %s: number expected after ':'\n", name, str);
            return false;
        }
        p = skip_ws(q);
    }

    if (*p != '>')
    {
        ERROR_LINE("field def syntax in %s /= %s: '>' expected after number\n", name, str);
        return false;
    }

    ++p;
    p = skip_ws(p);

    if (*p == ',')
    {
        ++p;
        p = skip_ws(p);

        char qual[32];
        p = parse_token(p, qual, sizeof(qual), 0, false);
        if (!p)
            return false;
        if (!qual[0])
        {
            ERROR_LINE("field syntax in %s /= %s: qualifier expected after ','\n", name, str);
            return false;
        }

        if (strcmp(qual, ".DEFAULT") == 0)
        {
            p = skip_ws(p);
            if (*p != '=')
            {
                ERROR_LINE("field syntax in %s /= %s: expected '=' after .DEFAULT\n", name, str);
                return false;
            }
            ++p;

            fdef.def_val = strtoul(p, &q, 16);
            if (q == p)  // couldn't parse any numbers
            {
                ERROR_LINE("field def syntax in %s /= %s: number expected after '='\n", name, str);
                return false;
            }

            p = skip_ws(q);
            if (*p != 0)
            {
                ERROR_LINE("field def syntax in %s /= %s: bad char %c after .DEFAULT=\n", name, str, *p);
                return false;
            }

            fdef.def_flag  = true;
            fdef.addr_flag = false;
            fdef.next_flag = false;
        }
        else if (strcmp(qual, ".ADDRESS") == 0)
        {
            if (*p != 0)
            {
                ERROR_LINE("field def syntax in %s /= %s: bad char %c after .ADDRESS=\n", name, str, *p);
                return false;
            }

            fdef.def_flag  = false;
            fdef.addr_flag = true;
            fdef.next_flag = false;
        }
        else if (strcmp(qual, ".NEXTADDRESS") == 0)
        {
            if (*p != 0)
            {
                ERROR_LINE("field def syntax in %s /= %s: bad char %c after .NEXTADDRESS=\n", name, str, *p);
                return false;
            }

            fdef.def_flag  = false;
            fdef.addr_flag = true;
            fdef.next_flag = true;
        }
        else
        {
            ERROR_LINE("field def syntax in %s /= %s: bad qualifier %s\n", name, str, qual);
            return false;
        }
    }

    if (*p != 0)
    {
        ERROR_LINE("field def syntax in %s /= %s: bad char %c after '>'\n", name, str, *p);
        return false;
    }

    DEBUG_PARSING("parsed field def %s: li=%u, ri=%u def=0x%0x flags=0b%03b\n", name,
            fdef.li, fdef.ri, fdef.def_val, (fdef.def_flag<<2|fdef.addr_flag<<1|fdef.next_flag));

    if (!handle_field_def(name, &fdef))
        return false;

    return true;
}

static hashtable_t blue_macros;

static char *expand_macro(char *xline, uint max, const char *macro_name, const char *macro, const args_t *args)
{
    const char *p = macro;
    char *q = xline;

    while (max > 1 && *p)
    {
        if (*p == '@')
        {
            uint ai = (*++p)-'1';
            if (ai >= MAXARGS)
            {
                ERROR_LINE("macro expansion bad arg: @%c\n", *p);
                return 0;
            }
            else if (ai >= args->nargs)
            {
                ERROR_LINE("macro expansion not enough arguments for @%u\n", ai+1);
                return 0;
            }
            const char *a = args->arg[ai];
            int len = strlen(a);
            if (len+1 > max)
            {
                ERROR_LINE("macro expansion buffer overflow on args\n");
                return 0;
            }
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
    DEBUG_MACROS("expanding macro %s%s", macro_name, args->nargs > 0 ? "(" : "");
    if ((debug_flags & 8) != 0)
    {
        if (args->nargs > 0)
        {
            for (uint di=0; di<args->nargs; ++di)
            {
                io_write_error_list("%s%s", args->arg[di], (di+1)<args->nargs ? "," : ")");
                fprintf(stderr,     "%s%s", args->arg[di], (di+1)<args->nargs ? "," : ")");
            }
        }
        io_write_error_list(" to \"%s\"\n", xline);
        fprintf(stderr,     " to \"%s\"\n", xline);
    }
#endif

    return q;
}

static bool expand_line_once(char *xline, uint max, const char *line, uint *num_expanded)
{
    char name[MAXLINE]; // could be entire line
    args_t args;

    if (num_expanded)
        *num_expanded = 0;

    hashtable_t local_macros;
    hashtable_init(&local_macros, 16);

    // handle DT/LONG vs. LONG problem: block macros matching correct field value
    // handle J/C.FORK problem: block any macros in field-value position
    bool isfval = false;

    const char *p = line;
    char *q = xline;
    while(*p && max > 1)
    {
        const char *pp = parse_token(p, name, sizeof(name), &args, true);
        if (!pp)
        {
            hashtable_free(&local_macros);
            return false;
        }

        if (name[0])
        {
            const char *macro = macro_get(name);

            // don't expand recursively, block all macros already seen
            // allow recursing on parameterized macros for now - params could be different
            bool isblue = hashtable_get_entry(&blue_macros, name) != 0;

            if (macro && (!isblue || args.nargs > 0) && !isfval)
            {
                // expand the macro and args if found
                char *r = expand_macro(q, max, name, macro, &args);
                if (!r)
                {
                    hashtable_free(&local_macros);
                    return false;
                }
                max -= (r-q);
                q = r;

                hashtable_addi(&local_macros, name,  1);
                if (num_expanded)
                    ++*num_expanded;
            }
            else
            {
                // otherwise copy original contents
                uint len = (pp - p);
                if (len+1 > max)
                {
                    ERROR_LINE("macro expansion buffer overflow\n");
                    hashtable_free(&local_macros);
                    return 0;
                }
                strncpy(q, p, len);
                q += len, max -= len;
                q = remove_trailing_ws(xline, q);
            }
        }

        p = pp;

        if (max <= 1)
        {
            ERROR_LINE("macro expansion overflow\n");
            hashtable_free(&local_macros);
            return false;
        }

        // track whether this might be a field-name/val pair
        isfval = (*p == '/');

        if (*p)
        {

            *q++ = *p++, --max;
            p = skip_ws(p);
        }
    }

    *q = 0;
    remove_trailing_ws(xline, q);

    hashtable_copy(&blue_macros, &local_macros);
    hashtable_free(&local_macros);
    return true;
}

bool expand_line(char *xline, uint max, const char *line)
{
    DEBUG_MACROS("expanding : %s\n", line);

    hashtable_init(&blue_macros, 16);

    char tmp[MAXLINE];
    tmp[sizeof(tmp)-1] = 0;
    for (uint i=0; i<MAXRECURSE; ++i)
    {
        strncpy(tmp, line, sizeof(tmp));
        if (tmp[sizeof(tmp)-1] != 0)
        {
            ERROR_LINE("macro expansion tmp buffer overflow\n");
            hashtable_free(&blue_macros);
            return false;
        }

        uint ne = 0;
        if (!expand_line_once(xline, max, tmp, &ne))
        {
            hashtable_free(&blue_macros);
            return false;
        }

        if (ne <= 0)
        {
            DEBUG_MACROS("expanded  : %s\n", xline);
            if (i > 0)
                io_write_expanded(xline);
            hashtable_free(&blue_macros);
            return true;
        }

        line = xline;
    }
    ERROR_LINE("macro expansion recursion limit hit\n");
    hashtable_free(&blue_macros);
    return false;
}

bool parse_microcode(const char *line)
{
    DEBUG_PARSING("parsing microcode: %s\n", line);

    char xline[MAXLINE];
    const char *p = xline;
    if (!expand_line(xline, sizeof(xline), line))
        return false;

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
        if (!name[0])
        {
            ERROR_LINE("ucode syntax: empty token at start or after '/' or ','\n");
            return false;
        }

        uint len = strlen(name)+1;
        if (len > max)
        {
            ERROR_LINE("ucode buffer overflow\n");
            return false;
        }

        strncpy(q, name, max);
        field[i].name = q;
        q += len;
        max -= len;

        if (*p != '/')
        {
            ERROR_LINE("ucode syntax: expected '/' after %s\n", name);
            return false;
        }

        p = skip_ws(p+1);

        p = parse_token(p, name, sizeof(name), 0, false);
        if (!p)
            return false;
        if (!name[0])
        {
            ERROR_LINE("ucode syntax: line truncated after '/'\n");
            return false;
        }

        if (isdigit(name[0]))
        {
            char *r = 0;
            field[i].valstr = 0;
            field[i].valint = strtoul(name, &r, 16);
            if (!r || *r) // couldn't parse all numbers
            {
                ERROR_LINE("ucode syntax: number format bad: %s\n", name);
                return false;
            }
        }
        else
        {
            len = strlen(name)+1;
            if (len > max)
            {
                ERROR_LINE("ucode buffer overflow\n");
                return false;
            }

            strncpy(q, name, max);
            field[i].valstr = q;
            field[i].valint = 0;
            q += len;
            max -= len;
        }

        if (!*p)
        {
            if(!handle_ucode(field, i+1))
                return false;

            return true;
        }
        else if (*p != ',')
        {
            ERROR_LINE("ucode syntax: expected ',' after %s\n", name);
            return false;
        }

        p = skip_ws(p+1);
    }

    ERROR_LINE("ucode too many fields\n");
    return false;
}

bool parse_line(const char *line)
{
    const char *p = skip_ws(line);

    char name[MAXNAME];
    p = parse_token(p, name, sizeof(name), 0, true);
    if (!p)
        return false;

    if (*p == '/' && p[1] == '=') // field def
    {
        p += 2; // '/='

        if (!parse_field_def(name, p))
            return false;
    }
    else if (*p == '=')
    {
        p = skip_ws(p+1); // '='

        if (name[0] != 0) // field val
        {
            const char *rhs = p;

            DEBUG_PARSING("parsing field val: %s = %s\n", name, rhs);
            char *q = 0;
            uint32_t val = strtoul(p, &q, 16);
            if (q == p) // couldn't parse any numbers
            {
                ERROR_LINE("field val syntax in %s = %s: expected number\n", name, rhs);
                return false;
            }

            p = skip_ws(q);
            if (*p)
            {
                ERROR_LINE("field val syntax in %s = %s: bad char %c after number\n", name, rhs, *p);
                return false;
            }

            DEBUG_PARSING("parsed field val: %s 0x%x\n", name, val);

            if (!handle_field_val(name, val))
                return false;
        }
        else // constraint
        {
            handle_field_def(0, 0); // no longer added fields

            constraint_t cst = { };

            p = parse_constraint(p, &cst);
            if (!p)
                return false;

            if (!handle_constraint(&cst))
                return false;

            // microcode can follow address/label on same line
            p = skip_ws(p);
            if (*p)
            {
                if (!parse_microcode(p))
                    return false;
            }
        }
    }
    else if (*p == ':') // addr or label
    {
        handle_field_def(0, 0); // no longer added fields
        DEBUG_PARSING("parsing address/label: %s\n", name);

        if (!name[0])
        {
            ERROR_LINE("label syntax: empty label before ':'\n");
            return false;
        }

        // if starts with a digit (0-9) it is an address, not label
        if (isdigit(name[0]))
        {
            const char *q = 0;
            uint32_t addr = strtoul(name, (char **)&q, 16);
            if (q == name) // couldn't parse any numbers
            {
                ERROR_LINE("addr syntax: expected number\n");
                return false;
            }

            q = skip_ws(q);
            if (*q)
            {
                ERROR_LINE("addr syntax: bad char %c after number\n", *p);
                return false;
            }

            DEBUG_PARSING("parsed address 0x%0x\n", addr);

            if (!handle_addr(addr))
                return false;
        }
        else
        {
            DEBUG_PARSING("parsed label %s\n", name);

            if (!handle_label(name))
                return false;
        }

        // microcode can follow address/label on same line
        p = skip_ws(p+1);
        if (*p)
        {
            if (!parse_microcode(p))
                return false;
        }
    }
    else if (name[0] == '.') // directive
    {
        handle_field_def(0, 0); // no longer added fields
        if (!parse_directive(name, p))
            return false;
    }
    else if (*p == '"') // macro
    {
        handle_field_def(0, 0); // no longer added fields
        DEBUG_PARSING("parsing macro: %s\n", line);

        // check trailing '"'
        const char *q = &p[strlen(p)-1];
        if (*q != '"')
        {
            ERROR_LINE("macro def syntax: expected trailing '\"'\n");
            return false;
        }

        DEBUG_PARSING("parsed macro: %s %s\n", name, p);

        if (!handle_macro_def(name, p))
            return false;
    }
    else  // microcode
    {
        handle_field_def(0, 0); // no longer added fields
        p = skip_ws(line); // reset to beginning of line, skipping whitespace
        if (!parse_microcode(p))
            return false;
    }

    return true;
}
