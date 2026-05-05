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

#define KEYWORD(_)                          \
    _(KEYWORD_OBJECT,       "object")       \
    _(KEYWORD_ARRAY,        "array")        \
    _(KEYWORD_STRING,       "string")       \
    _(KEYWORD_INTEGER,      "integer")      \
    _(KEYWORD_NUMBER,       "number")       \
    _(KEYWORD_BOOLEAN,      "boolean")      \
    _(KEYWORD_NULL,         "null")         \
    _(KEYWORD_PROPERTY,     "property")     \
    _(KEYWORD_ITEM,         "item")         \
    _(KEYWORD_PATTERN,      "pattern")      \
    _(KEYWORD_FORMAT,       "format")       \
    _(KEYWORD_MASK,         "mask")         \
    _(KEYWORD_MIN_LENGTH,   "minLength")    \
    _(KEYWORD_MAX_LENGTH,   "maxLength")    \
    _(KEYWORD_MIN,          "min")          \
    _(KEYWORD_MAX,          "max")

#define KEYWORD_ENUM(a, b) a,
enum { KEYWORD(KEYWORD_ENUM) NKEYWORDS, INVALID_KEYWORD };
#define KEYWORD_TEXT(a, b) b,
static const char *keywords[] = { KEYWORD(KEYWORD_TEXT) };

/******************************************************************************
 EVAL CODE
******************************************************************************/

typedef struct code
{
    int (*eval)(const struct code *, void *);
    union { char *string; double number; };
} code_t;

static int eval_code(const code_t *code, void *data)
{
    for (; code->eval != NULL; code++)
    {
        if (!code->eval(code, data))
        {
            return 0;
        }
    }
    return 1;
}

static int eval_type(const code_t *code, void *data)
{
    (void)data;
    printf("is_type: %u %s\n",
        (unsigned)code->number,
        keywords[(unsigned)code->number]
    );
    return 1;
}

static int eval_property(const code_t *code, void *data)
{
    (void)data;
    printf("property: %s\n", code->string);
    return 1;
}

static int eval_item(const code_t *code, void *data)
{
    (void)code;
    (void)data;
    printf("item\n");
    return 1;
}

static int eval_pattern(const code_t *code, void *data)
{
    (void)data;
    printf("pattern: %s\n", code->string);
    return 1;
}

static int eval_format(const code_t *code, void *data)
{
    (void)data;
    printf("format: %s\n", code->string);
    return 1;
}

static int eval_mask(const code_t *code, void *data)
{
    (void)data;
    printf("mask: %s\n", code->string);
    return 1;
}

static int eval_min_length(const code_t *code, void *data)
{
    (void)data;
    printf("minLength: %zu\n", (size_t)code->number);
    return 1;
}

static int eval_max_length(const code_t *code, void *data)
{
    (void)data;
    printf("maxLength: %zu\n", (size_t)code->number);
    return 1;
}

static int eval_min(const code_t *code, void *data)
{
    (void)data;
    printf("min: %f\n", code->number);
    return 1;
}

static int eval_max(const code_t *code, void *data)
{
    (void)data;
    printf("max: %f\n", code->number);
    return 1;
}

static int eval_dummy(const code_t *code, void *data)
{
    (void)code;
    (void)data;
    printf("...\n");
    return 1;
}

/******************************************************************************
 COMPILE CODE
******************************************************************************/

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
            return NULL;
        }
        pool->code = code;
        pool->room = room;
    }
    memset(&pool->code[pool->size], 0, sizeof(code_t));
    return &pool->code[pool->size++];
}

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

static int keyword_is_expected(const sexp_event_t *event, unsigned keyword)
{
    frame_t *frame = event->data;
    path_t *parent = event->depth ? &frame->path[event->depth - 1] : NULL;

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
                ? parent->keyword != KEYWORD_PROPERTY
                    ? parent->keyword == KEYWORD_ITEM
                    : parent->type == SEXP_STRING
                : 1;
        case KEYWORD_PROPERTY:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_OBJECT);
        case KEYWORD_ITEM:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_ARRAY);
        case KEYWORD_PATTERN:
        case KEYWORD_FORMAT:
        case KEYWORD_MASK:
        case KEYWORD_MIN_LENGTH:
        case KEYWORD_MAX_LENGTH:
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

static int keyword_is_valid(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth];
    path_t *parent = event->depth ? &path[-1] : NULL;

    switch (path->keyword)
    {
        case KEYWORD_PROPERTY:
        case KEYWORD_PATTERN:
        case KEYWORD_FORMAT:
        case KEYWORD_MASK:
            return path->type == SEXP_STRING;
        case KEYWORD_MIN_LENGTH:
        case KEYWORD_MAX_LENGTH:
            return path->type == SEXP_INTEGER;
        case KEYWORD_MIN:
        case KEYWORD_MAX:
            return parent->keyword == KEYWORD_INTEGER
                ? path->type == SEXP_INTEGER
                : (path->type & SEXP_NUMBER) != 0;
        default:
            return path->type == SEXP_UNDEFINED;
    }
}

static int push_symbol(const sexp_event_t *event)
{
    unsigned keyword = keyword_id(event->string);

    if (keyword == INVALID_KEYWORD)
    {
        return 0;
    }
    if (!keyword_is_expected(event, keyword))
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
        case KEYWORD_ARRAY:
        case KEYWORD_STRING:
        case KEYWORD_INTEGER:
        case KEYWORD_NUMBER:
        case KEYWORD_BOOLEAN:
        case KEYWORD_NULL:
            code->eval = eval_type;
            code->number = keyword;
            break;
        case KEYWORD_PROPERTY:
            code->eval = eval_property;
            break;
        case KEYWORD_ITEM:
            code->eval = eval_item;
            break;
        case KEYWORD_PATTERN:
            code->eval = eval_pattern;
            break;
        case KEYWORD_FORMAT:
            code->eval = eval_format;
            break;
        case KEYWORD_MASK:
            code->eval = eval_mask;
            break;
        case KEYWORD_MIN_LENGTH:
            code->eval = eval_min_length;
            break;
        case KEYWORD_MAX_LENGTH:
            code->eval = eval_max_length;
            break;
        case KEYWORD_MIN:
            code->eval = eval_min;
            break;
        case KEYWORD_MAX:
            code->eval = eval_max;
            break;
        default:
            code->eval = eval_dummy;
            break;
    }
    return 1;
}

static int push_symbol_end(const sexp_event_t *event)
{
    if (!keyword_is_valid(event))
    {
        return 0;
    }

    frame_t *frame = event->data;

    if (event->depth == 0)
    {
        return pool_resize(&frame->pool) != NULL;
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

