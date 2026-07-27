/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "clib_math.h"
#include "clib_unicode.h"
#include "json_parser.h"

/*
---------------------------------------------------------------------
Recursive-descent JSON parser
---------------------------------------------------------------------
Each parse_*() function consumes one JSON value and fires 'callback'
with the fully-populated event once per value (twice for objects and
arrays: JSON_OBJECT/JSON_ARRAY on open, JSON_OBJECT_END/JSON_ARRAY_END
on close). The parser holds no tree of its own — building one, or
anything else, is entirely up to the callback (see decode() in
json_writer.c for the tree builder actually used by json_decode()).

Strings are decoded in place: escape sequences are always shorter
than or equal to their source, so decode_string() overwrites the
input between the opening and closing quotes as it goes. This is why
json_parse() requires a writable buffer, not a string literal.
---------------------------------------------------------------------
*/

static int parse(json_event_t *);

static char *skip_spaces(char *str)
{
    while (is_space(*str))
    {
        str++;
    }
    return str;
}

static char *decode_string(json_event_t *event)
{
    char *str = ++event->iter, *ptr = str;

    while ((*str != '\"') && !is_cntrl(*str))
    {
        if (*str != '\\')
        {
            *ptr++ = *str++;
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

static int parse_key(json_event_t *event)
{
    if (*event->iter != '\"')
    {
        return 0;
    }
    if ((event->key = decode_string(event)) == NULL)
    {
        return 0;
    }
    if (*event->iter != ':')
    {
        return 0;
    }
    event->iter = skip_spaces(event->iter + 1);
    return 1;
}

static int parse_object(json_event_t *event)
{
    event->type = JSON_OBJECT;
    event->iter = skip_spaces(event->iter + 1);
    if (!event->callback(event))
    {
        return 0;
    }
    if (*event->iter != '}')
    {
        if (event->depth++ >= JSON_MAX_DEPTH)
        {
            return 0;
        }
        if (!parse_key(event) || !parse(event))
        {
            return 0;
        }
        while (*event->iter == ',')
        {
            event->iter = skip_spaces(event->iter + 1);
            if (!parse_key(event) || !parse(event))
            {
                return 0;
            }
        }
        event->depth--;
    }
    if (*event->iter != '}')
    {
        return 0;
    }
    event->key = NULL;
    event->type = JSON_OBJECT_END;
    event->iter = skip_spaces(event->iter + 1);
    return event->callback(event);
}

static int parse_array(json_event_t *event)
{
    event->type = JSON_ARRAY;
    event->iter = skip_spaces(event->iter + 1);
    if (!event->callback(event))
    {
        return 0;
    }
    if (*event->iter != ']')
    {
        if (event->depth++ >= JSON_MAX_DEPTH)
        {
            return 0;
        }
        event->key = NULL;
        if (!parse(event))
        {
            return 0;
        }
        while (*event->iter == ',')
        {
            event->iter = skip_spaces(event->iter + 1);
            if (!parse(event))
            {
                return 0;
            }
        }
        event->depth--;
    }
    if (*event->iter != ']')
    {
        return 0;
    }
    event->key = NULL;
    event->type = JSON_ARRAY_END;
    event->iter = skip_spaces(event->iter + 1);
    return event->callback(event);
}

static int parse_string(json_event_t *event)
{
    event->string = decode_string(event);
    if (event->string == NULL)
    {
        return 0;
    }
    event->type = JSON_STRING;
    return event->callback(event);
}

static int parse_number(json_event_t *event)
{
    char *end;

    errno = 0;

    double number = strtod(event->iter, &end);

    if ((event->iter == end) || (errno == ERANGE))
    {
        return 0;
    }
    event->number = number;
    // Differs from the standard, which only considers 'number' as a numeric type.
    // Here, we classify nodes as either 'integer' or 'real'.
    // Safe integers are numbers within the range of -(2^53-1) to +(2^53-1)
    if ((event->iter + strspn(event->iter, "-0123456789") >= end) &&
        IS_SAFE_INTEGER(number))
    {
        event->type = JSON_INTEGER;
    }
    else
    {
        event->type = JSON_REAL;
    }
    event->iter = skip_spaces(end);
    return event->callback(event);
}

static int parse_true(json_event_t *event)
{
    if (strncmp(event->iter, "true", 4))
    {
        return 0;
    }
    event->type = JSON_TRUE;
    event->iter = skip_spaces(event->iter + 4);
    return event->callback(event);
}

static int parse_false(json_event_t *event)
{
    if (strncmp(event->iter, "false", 5))
    {
        return 0;
    }
    event->type = JSON_FALSE;
    event->iter = skip_spaces(event->iter + 5);
    return event->callback(event);
}

static int parse_null(json_event_t *event)
{
    if (strncmp(event->iter, "null", 4))
    {
        return 0;
    }
    event->type = JSON_NULL;
    event->iter = skip_spaces(event->iter + 4);
    return event->callback(event);
}

static int parse(json_event_t *event)
{
    switch (*event->iter)
    {
        case '{':
            return parse_object(event);
        case '[':
            return parse_array(event);
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
int json_parse(char *iter, json_event_callback callback, void *data)
{
    if (iter == NULL)
    {
        return 0;
    }

    json_event_t event =
    {
        .iter = skip_spaces(iter),
        .callback = callback,
        .data = data
    };

    return parse(&event) && (*event.iter == '\0');
}

