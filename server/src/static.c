/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers.h"
#include "static.h"

static buffer_t no_content;
static buffer_t bad_request;
static buffer_t unauthorized;
static buffer_t not_found;
static buffer_t server_error;

#define load_buffer(buffer, method, content) \
    buffer_format(buffer, method \
        "Content-Type: text/plain\r\n" \
        "Content-Length: %zu\r\n\r\n%s", \
        strlen(content), content)
#define load_buffer_no_content(buffer, method) \
    buffer_write(buffer, method "\r\n")

static void load(void)
{
    if (!load_buffer_no_content(&no_content, HEADER_NO_CONTENT) ||
        !load_buffer(&bad_request, HEADER_BAD_REQUEST, "Bad Request") ||
        !load_buffer(&unauthorized, HEADER_UNAUTHORIZED, "Unauthorized") ||
        !load_buffer(&not_found, HEADER_NOT_FOUND, "Not Found") ||
        !load_buffer(&server_error, HEADER_SERVER_ERROR, "Internal Server Error"))
    {
        perror("load_buffer");
        exit(EXIT_FAILURE);
    }
}

static void unload(void)
{
    buffer_clear(&no_content);
    buffer_clear(&bad_request);
    buffer_clear(&unauthorized);
    buffer_clear(&not_found);
    buffer_clear(&server_error);
}

void static_load(void)
{
    atexit(unload);
    load();
}

void static_reload(void)
{
    unload();
    load();
}

const buffer_t *static_no_content(void)
{
    return &no_content;
}

const buffer_t *static_bad_request(void)
{
    return &bad_request;
}

const buffer_t *static_unauthorized(void)
{
    return &unauthorized;
}

const buffer_t *static_not_found(void)
{
    return &not_found;
}

const buffer_t *static_server_error(void)
{
    return &server_error;
}

