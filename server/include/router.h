/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef ROUTER_H
#define ROUTER_H

#include <stddef.h>

enum { STATEMENT_MODE_READ, STATEMENT_MODE_WRITE };

typedef struct
{
    const char *sql;
    size_t offset;
    size_t size;
    int mode;
} statement_t;

typedef struct
{
    const char *path;
    void *schema;
    statement_t statement;
    int index;
} endpoint_t;

int router_method(const char *);
unsigned router_methods(const char *);
void router_load(void);
void router_reload(void);
const endpoint_t *router_search(const char *, int);
int router_walk(int (*)(endpoint_t *));

#endif

