/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef SEXP_PARSER_H
#define SEXP_PARSER_H

#include "sexp_header.h"

typedef struct sexp_event
{
    char *iter;
    union { char *string; double number; };
    unsigned type, depth;
    int (*callback)(const struct sexp_event *);
    void *data;
} sexp_event_t;

typedef int (*sexp_event_callback)(const sexp_event_t *);
int sexp_parse(char *, sexp_event_callback, void *);

#endif

