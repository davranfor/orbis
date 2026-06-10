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

static buffer_t bad_request;
static buffer_t unauthorized;
static buffer_t server_error;

#define load_buffer(buffer, method, content) \
    buffer_format(buffer, method \
        "Content-Type: application/json\r\n" \
        "Content-Length: %zu\r\n\r\n%s", \
        strlen(content), content)

static void load(void)
{
    if (!load_buffer(&bad_request,  HEADER_BAD_REQUEST,  "{\"error\": \"Bad Request\"}") ||
        !load_buffer(&unauthorized, HEADER_UNAUTHORIZED, "{\"error\": \"Unauthorized\"}") ||
        !load_buffer(&server_error, HEADER_SERVER_ERROR, "{\"error\": \"Internal Server Error\"}"))
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

