/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef SESSION_H
#define SESSION_H

#ifdef ALLOW_INSECURE_TOKEN
#define COOKIE_CLEAR \
    "Set-Cookie: session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; " \
    "Path=/; HttpOnly; SameSite=Strict\r\n"
#else
#define COOKIE_CLEAR \
    "Set-Cookie: session=; Expires=Thu, 01 Jan 1970 00:00:00 GMT; " \
    "Path=/; Secure; HttpOnly; SameSite=Strict\r\n"
#endif

#define TOKEN_MAX_LENGTH 64
#define COOKIE_SIZE 256

typedef struct
{
    int user;
    int role;
    char token[TOKEN_MAX_LENGTH + 1];
    char cookie[COOKIE_SIZE];
} session_t;

int session_parse(session_t *, const char *, char *);
const char *session_build(session_t *, int, int, const char *);
const char *session_clear(session_t *);

#endif

