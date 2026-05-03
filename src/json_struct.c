/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include "sexp_parser.h"
#include "json_private.h"
#include "json_struct.h"

#if 0

/**
 * Callback for json_parse.
 * Appends each node in DFS pre-order; containers track their index in
 * pool->depth[] so children can increment the parent's size.
 * node->child is initialised to NULL for containers: the fast path relies
 * on this for empty nested containers (size == 0, child must be NULL).
 */
static int build(const json_event_t *event)
{
    pool_t *pool = event->data;

    if (event->type & JSON_ITERABLE_END)
    {
        unsigned index = pool->depth[event->depth];

        if (pool->node[index].size > 0)
        {
            pool->node[index].span = pool->size - index;
        }
        return 1;
    }

    json_t *node;

    if (pool->size == pool->room)
    {
        unsigned room = pool->room == 0 ? 1 : pool->room * 2;

        node = realloc(pool->node, sizeof(*node) * room);
        if (node == NULL)
        {
            return 0;
        }
        pool->node = node;
        pool->room = room;
    }
    node = &pool->node[pool->size];
    node->key = event->key;
    switch (event->type)
    {
        case JSON_OBJECT:
        case JSON_ARRAY:
            pool->depth[event->depth] = pool->size;
            node->child = NULL;
            break;
        case JSON_STRING:
            node->string = event->string;
            break;
        case JSON_INTEGER:
        case JSON_REAL:
            node->number = event->number;
            break;
    }
    node->type = event->type;
    node->size = 0;
    pool->size++;
    if (event->depth > 0)
    {
        pool->node[pool->depth[event->depth - 1]].size++;
    }
    return 1;
}

#endif

typedef struct code
{
    int (*eval)(const struct code *, void *);
    union { size_t size; char *string; double number; };
} code_t;

typedef struct
{
    code_t *code;
    unsigned size, room;
    unsigned depth[SEXP_MAX_DEPTH];
} pool_t;

static int push_string(const code_t *code, void *data)
{
    (void)data;
    printf("string %s\n", code->string);
    return 1;
}

static int push_integer(const code_t *code, void *data)
{
    (void)data;
    printf("integer %.0f\n", code->number);
    return 1;
}

static int push_real(const code_t *code, void *data)
{
    (void)data;
    printf("real %g\n", code->number);
    return 1;
}

static int eval_code(const code_t *code, void *data)
{
    for (size_t i = 1; i < code->size; i++)
    {
        const code_t *node = &code[i];

        if (!node->eval(node, data))
        {
            return 0;
        }
    }
    return 1;
}

static int eval_dummy(const code_t *code, void *data)
{
    (void)code;
    (void)data;
    printf("...\n");
    return 1;
}

static int pool_init(pool_t *pool)
{
    pool->code = malloc(sizeof *pool->code);
    if (pool->code != NULL)
    {
        pool->code->eval = eval_code;
        pool->code->size = 0;
        pool->size = 1;
        pool->room = 1;
        return 1;
    }
    return 0;
}

static code_t *pool_resize(pool_t *pool)
{
    if (pool->size == pool->room)
    {
        unsigned room = pool->room * 2;
        code_t *code = realloc(pool->code, sizeof(*code) * room);

        if (code == NULL)
        {
            return 0;
        }
        pool->code = code;
        pool->room = room;
    }
    return &pool->code[pool->size++];
}

static int compile_dummy(const sexp_event_t *event)
{
    pool_t *pool = event->data;
    code_t *code = pool_resize(pool);

    if (code != NULL)
    {
        code->eval = eval_dummy;
        code->string = NULL;
        return 1;
    }
    return 0;
}

static int compile_string(const sexp_event_t *event)
{
    pool_t *pool = event->data;
    code_t *code = pool_resize(pool);

    if (code != NULL)
    {
        code->eval = push_string;
        code->string = event->string;
        return 1;
    }
    return 0;
}

static int compile_integer(const sexp_event_t *event)
{
    pool_t *pool = event->data;
    code_t *code = pool_resize(pool);

    if (code != NULL)
    {
        code->eval = push_integer;
        code->number = event->number;
        return 1;
    }
    return 0;
}

static int compile_real(const sexp_event_t *event)
{
    pool_t *pool = event->data;
    code_t *code = pool_resize(pool);

    if (code != NULL)
    {
        code->eval = push_real;
        code->number = event->number;
        return 1;
    }
    return 0;
}

static int compile(const sexp_event_t *event)
{
    switch (event->type)
    {
        case SEXP_SYMBOL:
            return compile_dummy(event);
        case SEXP_SYMBOL_END:
            return compile_dummy(event);
        case SEXP_STRING:
            return compile_string(event);
        case SEXP_INTEGER:
            return compile_integer(event);
        case SEXP_REAL:
            return compile_real(event);
        case SEXP_TRUE:
            return compile_dummy(event);
        case SEXP_FALSE:
            return compile_dummy(event);
        case SEXP_NULL:
            return compile_dummy(event);
        default:
            return 0;
    }
}

void *json_compile(char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    pool_t pool = { 0 };

    if (!pool_init(&pool))
    {
        return NULL;
    }
    if (sexp_parse(str, compile, &pool))
    {
        pool.code->size = pool.size;
        return pool.code;
    }
    free(pool.code);
    return NULL;
}

int json_validate(const json_t *node, const void *code)
{
    (void)node;
    return eval_code(code, NULL);
}

