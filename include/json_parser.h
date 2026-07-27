/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "json_header.h"

typedef struct json_event
{
    char *iter, *key;
    union { char *string; double number; };
    unsigned type, depth;
    int (*callback)(const struct json_event *);
    void *data;
} json_event_t;

typedef int (*json_event_callback)(const json_event_t *);
int json_parse(char *, json_event_callback, void *);

#endif

