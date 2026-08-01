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
static buffer_t server_error;

#define fill_content(title, issue) \
    "{\"title\": \"" title "\", \"issue\": \"" issue "\"}"
#define load_buffer(buffer, method, content) \
    buffer_format(buffer, method \
        "Content-Type: application/json\r\n" \
        "Content-Length: %zu\r\n\r\n%s", \
        strlen(content), content)

static void load(void)
{
    const char *bad_request_content  = fill_content("Bad Request", "Malformed request");
    const char *unauthorized_content = fill_content("Unauthorized", "Login required");
    const char *server_error_content = fill_content("Internal Server Error", "Out of memory");
 
    if (!load_buffer(&bad_request,  HEADER_BAD_REQUEST,  bad_request_content)  ||
        !load_buffer(&unauthorized, HEADER_UNAUTHORIZED, unauthorized_content) ||
        !load_buffer(&server_error, HEADER_SERVER_ERROR, server_error_content))
    {
        perror("load_buffer");
        exit(EXIT_FAILURE);
    }
}

static void unload(void)
{
    buffer_clear(&bad_request);
    buffer_clear(&unauthorized);
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

const buffer_t *static_bad_request(void)
{
    return &bad_request;
}

const buffer_t *static_unauthorized(void)
{
    return &unauthorized;
}

const buffer_t *static_server_error(void)
{
    return &server_error;
}

