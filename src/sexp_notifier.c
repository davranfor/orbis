/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stddef.h>
#include "sexp_parser.h"
#include "sexp_notifier.h"

static int compile(const sexp_event_t *event)
{
    switch (event->type)
    {
/*
        case SEXP_SYMBOL:
            return push_symbol(event);
        case SEXP_REDUCE:
            return push_reduce(event);
        case SEXP_STRING:
        case SEXP_INTEGER:
        case SEXP_REAL:
            return push_scalar(event);
*/
        default:
            return 1;
    }
}

void *sexp_compile(char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

/*
    frame_t frame = { 0 };

    if (!sexp_parse(str, compile, &frame))
    {
        free(frame.code);
        return NULL;
    }
    return frame.code;
*/
    return NULL;
}

int sexp_bind(const void *code, sexp_bind_callback callback, void *data)
{
/*
    schema_t schema =
    {
        .node = node,
        .callback = callback,
        .data = data
    };

    if ((node == NULL) || (code == NULL))
    {
        return 0;
    }
    return eval_code(code, &schema);
*/
    (void)code;
    (void)callback;
    (void)data;
    return 1;
}

