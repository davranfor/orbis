/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers.h"
#include "static.h"

static buffer_t bad_request;
static buffer_t unauthorized;
static buffer_t internal_server_error;

#define fill_content(title, issue) \
    "{\"title\": \"" title "\", \"issue\": \"" issue "\"}"
#define load_buffer(buffer, method, content) \
    buffer_format(buffer, method \
        "Content-Type: application/json\r\n" \
        "Content-Length: %zu\r\n\r\n%s", \
        strlen(content), content)

static void load(void)
{
    const char *content[] =
    {
        fill_content("Bad Request", "Malformed request"),
        fill_content("Unauthorized", "Login required"),
        fill_content("Internal Server Error", "Out of memory")
    };
 
    if (!load_buffer(&bad_request, HEADER_BAD_REQUEST, content[0]) ||
        !load_buffer(&unauthorized, HEADER_UNAUTHORIZED, content[1]) ||
        !load_buffer(&internal_server_error, HEADER_INTERNAL_SERVER_ERROR, content[2]))
    {
        perror("load_buffer");
        exit(EXIT_FAILURE);
    }
}

static void unload(void)
{
    buffer_clear(&bad_request);
    buffer_clear(&unauthorized);
    buffer_clear(&internal_server_error);
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

const buffer_t *static_bad_request(void)
{
    return &bad_request;
}

const buffer_t *static_unauthorized(void)
{
    return &unauthorized;
}

const buffer_t *static_internal_server_error(void)
{
    return &internal_server_error;
}

