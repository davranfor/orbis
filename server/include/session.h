/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef SESSION_H
#define SESSION_H

#define TOKEN_LENGTH 64
#define COOKIE_SIZE 256

typedef struct
{
    int user;
    int role;
    char token[TOKEN_LENGTH + 1];
    char cookie[COOKIE_SIZE];
} session_t;

int session_parse(session_t *, const char *, char *);
const char *session_build(session_t *, int, int, const char *);

#endif

