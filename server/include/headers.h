/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef HEADERS_H
#define HEADERS_H

#include "session.h"

#define HEADER_OK \
    "HTTP/1.1 200 OK\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_CREATED \
    "HTTP/1.1 201 Created\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_NO_CONTENT \
    "HTTP/1.1 204 No Content\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_BAD_REQUEST \
    "HTTP/1.1 400 Bad Request\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_UNAUTHORIZED \
    "HTTP/1.1 401 Unauthorized\r\n" \
    "Cache-Control: no-store\r\n" \
    COOKIE_CLEAR

#define HEADER_FORBIDDEN \
    "HTTP/1.1 403 Forbidden\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_NOT_FOUND \
    "HTTP/1.1 404 Not Found\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_METHOD_NOT_ALLOWED \
    "HTTP/1.1 405 Method Not Allowed\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_INTERNAL_SERVER_ERROR \
    "HTTP/1.1 500 Internal Server Error\r\n" \
    "Cache-Control: no-store\r\n"

#endif

