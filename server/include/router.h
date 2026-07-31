/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef ROUTER_H
#define ROUTER_H

typedef struct
{
    const char *path, *stmt;
    void *code;
    int index;
} endpoint_t;

int router_method(const char *);
unsigned router_methods(const char *);
void router_load(void);
void router_reload(void);
const endpoint_t *router_search(const char *, int);

#endif

