/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/clib_string.h>
#include <orbis/clib_unicode.h>
#include <orbis/json_private.h>
#include <orbis/json_header.h>
#include <orbis/json_parser.h>
#include <orbis/json_buffer.h>
#include "headers.h"
#include "session.h"
#include "config.h"
#include "router.h"
#include "static.h"
#include "solver.h"
#include "parser.h"

/**
 * Tells the caller (server.c) whether 'message' is a complete HTTP
 * request yet: 0 means malformed/oversized (drop the connection),
 * -1 means keep reading, 1 means it's ready for parser_handle().
 * Needs no state of its own: headers end at the first "\r\n\r\n", and
 * from there Content-Length (if present) says exactly how many body
 * bytes to wait for.
 */
int parser_status(const char *message, size_t length)
{
    if (length > REQUEST_MAX_LENGTH)
    {
        return 0;
    }

    const char *delimiter = strstr(message, "\r\n\r\n");

    if (delimiter == NULL)
    {
        return length > HEADERS_MAX_LENGTH ? 0 : -1;
    }

    size_t headers_length = (size_t)(delimiter - message);
    const char *length_mark = string_search(
        message, headers_length, "\r\nContent-Length:", 17
    );

    if (length_mark == NULL)
    {
        return delimiter[4] == '\0';
    }

    size_t content_length = strtoul(length_mark + 17, NULL, 10);
    size_t request_length = headers_length + content_length + 4;

    return length < request_length ? -1 : length == request_length;
}

typedef struct
{
    char *path, *params, *content;
    int content_is_parseable;
    session_t session;
} request_t;

static const buffer_t *parse_headers(request_t *request, char *str)
{
#ifdef DEBUG
    puts(str);
#endif

    /* Cuts 'str' right after the blank line, so it ends there as its
       own C string and router_method()/strstr() below never scan
       past the headers into the body */
    char *content = strstr(str, "\r\n\r\n") + 4;

    content[-2] = '\0';
    if (*content != '\0')
    {
        if (strstr(str, "\r\nContent-Type: application/json\r\n"))
        {
            request->content_is_parseable = 1; 
        }
        request->content = content;
    }
    if (router_method(str) != 0)
    {
        char *end = strchr(strchr(str, ' ') + 1, ' ');

        if (end == NULL)
        {
            return static_bad_request();
        }
        *end++ = '\0';
        request->path = str;
        request->params = strchr(str, '?');
        if (request->params != NULL)
        {
            *request->params++ = '\0';
        }
        if (!session_parse(&request->session, str, end))
        {
            return static_unauthorized();
        }
        return NULL;
    }
    return static_bad_request();
}

/**
 * Decodes a "k1=v1&k2=v2" query string into 'params' in one pass,
 * in place: percent-escapes and '+' shrink as they're decoded, so
 * the write cursor 'ptr' never runs ahead of the read cursor 'str'.
 * '=' and '&'/'\0' are where a key or value ends — each occurrence
 * NUL-terminates it in place and starts the next one right after.
 * A key with no '=' (params[size].key still NULL when '&' or '\0'
 * is hit) is treated as malformed, not as a valueless param.
 */
static unsigned decode_params(json_t *params, char *str)
{
    char *key = str, *ptr = str;
    unsigned size = 0;

    while (size < REQUEST_MAX_PARAMS)
    {
        if ((str[0] == '%') && is_xdigit(str[1]) && is_xdigit(str[2]))
        {
            int hi = hex_to_dec(str[1]);
            int lo = hex_to_dec(str[2]);

            *ptr++ = (char)((hi << 4) | lo);
            str += 3;
        }
        else if (str[0] == '+')
        {
            *ptr++ = ' ';
            str++;
        }
        else if (str[0] == '=')
        {
            if (params[size].key != NULL)
            {
                return 0;
            }
            params[size].key = key;
            params[size].string = ptr + 1;
            params[size].type = JSON_STRING;
            *ptr++ = '\0';
            str++;
        }
        else if (str[0] == '&')
        {
            if (params[size++].key == NULL)
            {
                return 0;
            }
            key = ptr + 1;
            *ptr++ = '\0';
            str++;
        }
        else if (str[0] == '\0')
        {
            if (params[size++].key == NULL)
            {
                return 0;
            }
            *ptr = '\0';
            return size;
        }
        else
        {
            *ptr++ = *str++;
        }
    }
    return 0;
}

static int parse_params(request_t *request, json_t *params)
{
    if (request->params == NULL)
    {
        params->type = JSON_NULL;
        return 1;
    }
    params->type = JSON_OBJECT;
    params->size = decode_params(params->child, request->params);
    return params->size > 0;
}

static int decode_fields(const json_event_t *event)
{
    json_t *fields = event->data;

    if (event->depth == 0)
    {
        if (event->type & JSON_ITERABLE)
        {
            fields->type = event->type;
            return 1;
        }
        if (event->type & JSON_ITERABLE_END)
        {
            return 1;
        }
        return 0;
    }
    if (event->depth == 1)
    {
        if (!(event->type & JSON_SCALAR))
        {
            return 0;
        }
        if (fields->size == REQUEST_MAX_FIELDS)
        {
            return 0;
        }

        json_t *child = &fields->child[fields->size++];

        child->key = event->key;
        child->type = event->type;
        switch (child->type)
        {
            case JSON_STRING:
                child->string = event->string;
                break;
            case JSON_INTEGER:
            case JSON_REAL:
                child->number = event->number;
                break;
        }
        return 1;
    }
    return 0;
}

static int parse_fields(request_t *request, json_t *fields)
{
    if (request->content == NULL)
    {
        fields->type = JSON_NULL;
        return 1;
    }
    if (request->content_is_parseable == 0)
    {
        fields->string = request->content;
        fields->type = JSON_STRING;
        return 1;
    }
    return json_parse(request->content, decode_fields, fields);
}

/**
 * Query string params, JSON/form body and session are three different
 * sources with three different formats; here they're assembled into
 * one synthetic json_t tree (all on the stack, no parsing of a real
 * document) so the rest of the pipeline — the "@eval" schema and SQL
 * placeholders in endpoints.sql — only ever has to deal with one
 * shape: node.params, node.content, node.session.
 */
const buffer_t *parser_handle(char *message)
{
    request_t request = { 0 };
    const buffer_t *buffer = parse_headers(&request, message);

    if (buffer != NULL)
    {
        return buffer;
    }

    json_t node =
    {
        .child = (json_t [])
        {
            {
                .key = "params",
                .child = (json_t [REQUEST_MAX_PARAMS]){{ 0 }},
            },
            {
                .key = "content",
                .child = (json_t [REQUEST_MAX_FIELDS]){{ 0 }},
            },
            {
                .key = "session",
                .child = (json_t [])
                {
                    {
                        .key = "user",
                        .number = request.session.user,
                        .type = JSON_INTEGER
                    },
                    {
                        .key = "role",
                        .number = request.session.role,
                        .type = JSON_INTEGER
                    }
                },
                .type = JSON_OBJECT,
                .size = 2
            }
        },
        .type = JSON_OBJECT,
        .size = 3
    };

    if (!parse_params(&request, &node.child[0]) ||
        !parse_fields(&request, &node.child[1]))
    {
        return static_bad_request();
    }
    return solver_handle(&node, request.path, &request.session);
}

