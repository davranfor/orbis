/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef HEADERS_H
#define HEADERS_H

#define HEADERS_MAX_LENGTH 4096
#define REQUEST_MAX_LENGTH (1024 * 1024 * 4)
#define REQUEST_MAX_PARAMS 16
#define REQUEST_MAX_FIELDS 64

#define HEADER_OK \
    "HTTP/1.1 200 OK\r\n" \
    "Cache-Control: no-store\r\n"

#define HEADER_CREATED \
    "HTTP/1.1 201 Created\r\n"

#define HEADER_NO_CONTENT \
    "HTTP/1.1 204 No Content\r\n"

#define HEADER_BAD_REQUEST \
    "HTTP/1.1 400 Bad Request\r\n"

#ifdef ALLOW_INSECURE_TOKEN
#define HEADER_UNAUTHORIZED \
    "HTTP/1.1 401 Unauthorized\r\n" \
    "Set-Cookie: session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/; HttpOnly; SameSite=Strict\r\n"
#else
#define HEADER_UNAUTHORIZED \
    "HTTP/1.1 401 Unauthorized\r\n" \
    "Set-Cookie: session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Path=/; Secure; HttpOnly; SameSite=Strict\r\n"
#endif

#define HEADER_FORBIDDEN \
    "HTTP/1.1 403 Forbidden\r\n"

#define HEADER_NOT_FOUND \
    "HTTP/1.1 404 Not Found\r\n"

#define HEADER_SERVER_ERROR \
    "HTTP/1.1 500 Internal Server Error\r\n"

#endif

