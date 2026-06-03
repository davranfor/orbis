/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef SESSION_H
#define SESSION_H

#define SESSION_SIZE 128

enum { SESSION_USER, SESSION_ROLE, SESSION_TOKEN, SESSION_VALUE };

typedef struct { int user, role; char *token; } session_t;

int session_parse(session_t *, const char *, char *);
int session_create(int, int, const char *, char *);

#endif

