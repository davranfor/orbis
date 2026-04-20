/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include "clib_math.h"
#include "clib_unicode.h"
#include "sexp_parser.h"

static int parse(sexp_event_t *);

static char *skip_spaces(char *str)
{
    while (is_space(*str))
    {
        str++;
    }
    return str;
}

static char *decode_string(sexp_event_t *event)
{
    char *str = ++event->iter, *ptr = str;

    while ((*str != '\"') && !is_cntrl(*str))
    {
        if (*str != '\\')
        {
            *ptr++ = *str;
            str += 1;
        }
        else if (is_esc(str + 1))
        {
            *ptr++ = decode_esc(str + 1);
            str += 2;
        }
        else if (is_hex(str + 1))
        {
            ptr += decode_hex(str + 2, ptr);
            str += 6;
        }
        else
        {
            return NULL;
        }
    }

    char *result = NULL;

    if (*str == '\"')
    {
        *ptr = '\0';
        result = event->iter;
        event->iter = skip_spaces(str + 1);
    }
    return result;
}

static int parse_keyword(sexp_event_t *event)
{
    if (!is_alpha(*event->iter))
    {
        return 0;
    }

    char *str = event->iter;
    char *ptr = event->string = str - 1;

    while (is_alpha(*str))
    {
        *ptr++ = *str++;
    }
    *ptr = '\0';
    event->iter = skip_spaces(str);
    return 1;
}

static int parse_symbol(sexp_event_t *event)
{
    event->type = SEXP_SYMBOL;
    event->iter = skip_spaces(event->iter + 1);
    if (!parse_keyword(event))
    {
        return 0;
    }
    if (!event->callback(event))
    {
        return 0;
    }
    if (*event->iter != ')')
    {
        if (event->depth++ >= SEXP_MAX_DEPTH)
        {
            return 0;
        }
        if (!parse(event))
        {
            return 0;
        }
        while (*event->iter != ')')
        {
            if (is_alnum(event->iter[-1]))
            {
                return 0;
            }
            if (!parse(event))
            {
                return 0;
            }
        }
        event->depth--;
    }
    if (*event->iter != ')')
    {
        return 0;
    }
    event->type = SEXP_SYMBOL_END;
    event->iter = skip_spaces(event->iter + 1);
    return event->callback(event);
}

static int parse_string(sexp_event_t *event)
{
    event->string = decode_string(event);
    if (event->string == NULL)
    {
        return 0;
    }
    event->type = SEXP_STRING;
    return event->callback(event);
}

static int parse_number(sexp_event_t *event)
{
    char *end;

    errno = 0;

    double number = strtod(event->iter, &end);

    if ((errno == ERANGE) || isnan(number) || isinf(number))
    {
        return 0;
    }
    event->number = number;
    // Differs from the standard, which only considers 'number' as a numeric type.
    // Here, we classify nodes as either 'integer' or 'real'.
    // Safe integers are numbers within the range of -2^52 to +2^52 (inclusive)
    if ((event->iter + strspn(event->iter, "-0123456789") >= end) && IS_SAFE_INTEGER(number))
    {
        event->type = SEXP_INTEGER;
    }
    else
    {
        event->type = SEXP_REAL;
    }
    event->iter = skip_spaces(end);
    return event->callback(event);
}

static int parse_true(sexp_event_t *event)
{
    if (strncmp(event->iter, "true", 4))
    {
        return 0;
    }
    event->type = SEXP_TRUE;
    event->iter = skip_spaces(event->iter + 4);
    return event->callback(event);
}

static int parse_false(sexp_event_t *event)
{
    if (strncmp(event->iter, "false", 5))
    {
        return 0;
    }
    event->type = SEXP_FALSE;
    event->iter = skip_spaces(event->iter + 5);
    return event->callback(event);
}

static int parse_null(sexp_event_t *event)
{
    if (strncmp(event->iter, "null", 4))
    {
        return 0;
    }
    event->type = SEXP_NULL;
    event->iter = skip_spaces(event->iter + 4);
    return event->callback(event);
}

static int parse(sexp_event_t *event)
{
    switch (*event->iter)
    {
        case '(':
            return parse_symbol(event);
        case '"':
            return parse_string(event);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return parse_number(event);
        case 't':
            return parse_true(event);
        case 'f':
            return parse_false(event);
        case 'n':
            return parse_null(event);
        default:
            return 0;
    }
}

/**
 * 'iter' must be writable (not a string literal).
 * The function modifies it in-place during parsing.
 */
int sexp_parse(char *iter, sexp_event_callback callback, void *data)
{
    if (iter == NULL)
    {
        return 0;
    }

    sexp_event_t event =
    {
        .iter = skip_spaces(iter),
        .callback = callback,
        .data = data
    };

    return (*event.iter == '(') && parse(&event) && (*event.iter == '\0');
}

