/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sexp_parser.h"
#include "json_private.h"
#include "json_struct.h"

typedef struct code
{
    int (*eval)(const struct code *, void *);
    union { char *string; double number; };
} code_t;

static int eval_code(const code_t *code, void *data)
{
    while (code->eval != NULL)
    {
        if (!code->eval(code, data))
        {
            return 0;
        }
        code++;
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

typedef struct { code_t *code; unsigned size, room; } pool_t;
typedef struct { unsigned index, keyword, type, size; } path_t;
typedef struct { pool_t pool; path_t path[SEXP_MAX_DEPTH]; } frame_t;

static code_t *pool_resize(pool_t *pool)
{
    if (pool->size == pool->room)
    {
        unsigned room = pool->room ? pool->room * 2 : 1;
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

// X macro indexing enum and array of strings containing keywords
#define KEYWORD(_)                  \
    _(KEYWORD_OBJECT,   "object")   \
    _(KEYWORD_ARRAY,    "array")    \
    _(KEYWORD_STRING,   "string")   \
    _(KEYWORD_INTEGER,  "integer")  \
    _(KEYWORD_NUMBER,   "number")   \
    _(KEYWORD_BOOLEAN,  "boolean")  \
    _(KEYWORD_NULL,     "null")     \
    _(KEYWORD_PROPERTY, "property") \
    _(KEYWORD_ITEM,     "item")     \
    _(KEYWORD_PATTERN,  "pattern")  \
    _(KEYWORD_MIN,      "min")      \
    _(KEYWORD_MAX,      "max")

#define KEYWORD_ENUM(a, b) a,
enum { KEYWORD(KEYWORD_ENUM) NKEYWORDS, INVALID_KEYWORD };
#define KEYWORD_TEXT(a, b) b,
static const char *keywords[] = { KEYWORD(KEYWORD_TEXT) };

static unsigned keyword_id(const char *keyword)
{
    for (unsigned i = 0; i < NKEYWORDS; i++)
    {
        if (!strcmp(keywords[i], keyword))
        {
            return i;
        }
    }
    printf("keyword %s not found\n", keyword);
    return INVALID_KEYWORD;
}

static int keyword_expected(const sexp_event_t *event, unsigned keyword)
{
    printf("keyword %s at depth %u\n", keywords[keyword], event->depth); 
    frame_t *frame = event->data;
    path_t *parent = event->depth ? &frame->path[event->depth - 1] : NULL;

    if ((parent != NULL) && (parent->keyword == KEYWORD_PROPERTY))
    {
        if (parent->type == SEXP_UNDEFINED)
        {
            return 0;
        }
    }
    switch (keyword)
    {
        case KEYWORD_OBJECT:
        case KEYWORD_ARRAY:
        case KEYWORD_STRING:
        case KEYWORD_INTEGER:
        case KEYWORD_NUMBER:
        case KEYWORD_BOOLEAN:
        case KEYWORD_NULL:
            return parent != NULL
                ? (parent->keyword == KEYWORD_PROPERTY) ||
                  (parent->keyword == KEYWORD_ITEM)
                : 1;
        case KEYWORD_PROPERTY:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_OBJECT);
        case KEYWORD_ITEM:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_ARRAY);
        case KEYWORD_PATTERN:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_STRING);
        case KEYWORD_MIN:
        case KEYWORD_MAX:
            return parent != NULL
                ? (parent->keyword == KEYWORD_INTEGER) ||
                  (parent->keyword == KEYWORD_NUMBER)
                : 0;
        default:
            return 0;
    }
}

static int push_symbol(const sexp_event_t *event)
{
    unsigned keyword = keyword_id(event->string);

    if (keyword == INVALID_KEYWORD)
    {
        return 0;
    }
    if (!keyword_expected(event, keyword))
    {
        printf("Unexpected keyword %s\n", keywords[keyword]); 
        return 0;
    }

    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth];

    path->index = frame->pool.size;
    path->keyword = keyword;
    path->type = SEXP_UNDEFINED;
    path->size = 0;

    code_t *code = pool_resize(&frame->pool);

    if (code == NULL)
    {
        return 0;
    }
    if (event->depth > 0)
    {
        path[-1].size++;
    }
    switch (keyword)
    {
        case KEYWORD_OBJECT:
        default:
            code->eval = eval_dummy;
            break;
    }
    code->string = NULL;
    return 1;
}

static int push_symbol_end(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    code_t *code;

    if (event->depth == 0)
    {
        code = pool_resize(&frame->pool);
        if (code == NULL)
        {
            return 0;
        }
        code->eval = NULL;
        code->string = NULL;
    }
    return 1;
}

static int push_string(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth - 1];
    code_t *code = &frame->pool.code[path->index];
    
    if (path->type != SEXP_UNDEFINED)
    {
        return 0;
    }
    path->type = SEXP_STRING;
    code->string = event->string;
    return 1;
}

static int push_integer(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth - 1];
    code_t *code = &frame->pool.code[path->index];
    
    if (path->type != SEXP_UNDEFINED)
    {
        return 0;
    }
    path->type = SEXP_INTEGER;
    code->number = event->number;
    return 1;
}

static int push_real(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth - 1];
    code_t *code = &frame->pool.code[path->index];
    
    if (path->type != SEXP_UNDEFINED)
    {
        return 0;
    }
    path->type = SEXP_NUMBER;
    code->number = event->number;
    return 1;
}

/*
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
*/

static int compile(const sexp_event_t *event)
{
    switch (event->type)
    {
        case SEXP_SYMBOL:
            return push_symbol(event);
        case SEXP_SYMBOL_END:
            return push_symbol_end(event);
        case SEXP_STRING:
            return push_string(event);
        case SEXP_INTEGER:
            return push_integer(event);
        case SEXP_REAL:
            return push_real(event);
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

    frame_t frame = { 0 };

    if (!sexp_parse(str, compile, &frame))
    {
        free(frame.pool.code);
        return NULL;
    }
    return frame.pool.code;
}

int json_validate(const json_t *node, const void *code)
{
    (void)node;
    return eval_code(code, NULL);
}

