/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef ROUTER_H
#define ROUTER_H

enum { GET = 1, POST, PUT, PATCH, DELETE };

typedef struct
{
    char *path;
    void *code;
    char *stmt;
    int index;
} endpoint_t;

int router_method(const char *);
void router_load(void);
void router_reload(void);
const endpoint_t *router_search(char *, int);

#endif

