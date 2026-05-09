/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "clib_string.h"
#include "clib_regex.h"
#include "clib_match.h"
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
    _(KEYWORD_MAX,          "max")          \
    _(KEYWORD_MULTIPLE_OF,  "multipleOf")

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
    return INVALID_KEYWORD;
}

static int keyword_is_type(unsigned keyword)
{
    switch (keyword)
    {
        case KEYWORD_OBJECT:
        case KEYWORD_ARRAY:
        case KEYWORD_STRING:
        case KEYWORD_INTEGER:
        case KEYWORD_NUMBER:
        case KEYWORD_BOOLEAN:
        case KEYWORD_NULL:
        case KEYWORD_PROPERTY:
        case KEYWORD_ITEM:
            return 1;
        default:
            return 0;
    } 
}

/******************************************************************************
 EVAL CODE
******************************************************************************/

typedef struct
{
    const json_t *node;
    const json_t *path[JSON_MAX_DEPTH];
    unsigned item[JSON_MAX_DEPTH];
    unsigned depth;
    json_validate_callback callback;
    void *data;
} schema_t;

typedef struct code
{
    int (*action)(const struct code *, schema_t *schema);
    union { char *string; double number; unsigned pair[2]; };
} code_t;

static int eval_code(const code_t *code, schema_t *schema)
{
    for (int iter = 0; code->action != NULL; code += iter)
    {
        if ((iter = code->action(code, schema)) == 0)
        {
            return 0;
        }
    }
    return 1;
}

static int eval_object(const code_t *code, schema_t *schema)
{
    printf("object: %u properties\nadditionalProperties: %s\n",
        code->pair[0],
        code->pair[1] == -1u ? "true" : "false");

    if (schema->node->type != JSON_OBJECT)
    {
        return 0;
    }

    int valid = code->pair[1] == -1u
        ? schema->node->size >= code->pair[0]
        : schema->node->size == code->pair[0];

    if (valid)
    {
        schema->path[schema->depth++] = schema->node;
    }
    return valid;
}

static int eval_object_end(const code_t *code, schema_t *schema)
{
    (void)code;
    schema->node = schema->path[--schema->depth];
    return 1;
}

static int eval_tuple(const code_t *code, schema_t *schema)
{
    printf("tuple\n");

    if (schema->node->type != JSON_ARRAY)
    {
        return 0;
    }

    printf("size: %u\n", (unsigned)code->number);

    if (schema->node->size != (unsigned)code->number)
    {
        return 0;
    }
    schema->path[schema->depth] = schema->node;
    schema->item[schema->depth] = 0;
    schema->depth++;
    return 1;
}

static int eval_tuple_end(const code_t *code, schema_t *schema)
{
    (void)code;
    schema->node = schema->path[--schema->depth];
    return 1;
}

static int eval_array(const code_t *code, schema_t *schema)
{
    printf("array\n");

    (void)code;
    if (schema->node->type != JSON_ARRAY)
    {
        return 0;
    }
    if (schema->node->size == 0)
    {
        return 0;
    }
    schema->path[schema->depth] = schema->node;
    schema->item[schema->depth] = 0;
    schema->depth++;
    return 1;
}

static int eval_array_end(const code_t *code, schema_t *schema)
{
    const json_t *array = schema->path[schema->depth - 1]; 

    if (schema->item[schema->depth - 1] == array->size)
    {
        schema->node = schema->path[--schema->depth];
        return 1;
    }
    return -(int)code->number;
}

static int eval_string(const code_t *code, schema_t *schema)
{
    printf("string\n");

    if (schema->node->type != JSON_STRING)
    {
        return 0;
    }

    unsigned min_length = code->pair[0];
    unsigned max_length = code->pair[1];

    if ((min_length > 0) || (max_length < -1u))
    {
        size_t length = string_length(schema->node->string);

        printf("minLength: %u\n", min_length);
        printf("maxLength: %u\n", max_length);
        return (length >= min_length) && (length <= max_length);
    }
    return 1;
}

static int eval_integer(const code_t *code, schema_t *schema)
{
    printf("integer\n");

    (void)code;
    return schema->node->type == JSON_INTEGER;
}

static int eval_number(const code_t *code, schema_t *schema)
{
    printf("number\n");

    (void)code;
    return (schema->node->type & JSON_NUMBER) != 0;
}

static int eval_boolean(const code_t *code, schema_t *schema)
{
    printf("boolean\n");

    (void)code;
    return (schema->node->type & JSON_BOOLEAN) != 0;
}

static int eval_null(const code_t *code, schema_t *schema)
{
    printf("null\n");

    (void)code;
    return schema->node->type == JSON_NULL;
}

static int eval_property(const code_t *code, schema_t *schema)
{
    printf("property: %s\n", code->string);

    const json_t *object = schema->path[schema->depth - 1]; 

    for (unsigned index = 0; index < object->size; index++)
    {
        if (!strcmp(object->child[index].key, code->string))
        {
            schema->node = &object->child[index];
            return 1;
        }
    }
    return 0;
}

static int eval_item(const code_t *code, schema_t *schema)
{
    printf("item: %u\n", schema->item[schema->depth - 1]);

    (void)code;

    const json_t *array = schema->path[schema->depth - 1]; 
    unsigned *index = &schema->item[schema->depth - 1];

    if (array->size > *index)
    {
        schema->node = &array->child[*index];
        (*index)++;
        return 1;
    }
    return 0;
}

static int eval_pattern(const code_t *code, schema_t *schema)
{
    printf("pattern: %s\n", code->string);

    return test_regex(schema->node->string, code->string);
}

static int eval_format(const code_t *code, schema_t *schema)
{
    printf("format: %s\n", code->string);

    return test_match(schema->node->string, code->string);
}

static int eval_mask(const code_t *code, schema_t *schema)
{
    printf("mask: %s\n", code->string);

    return test_mask(schema->node->string, code->string) != NULL;
}

static int eval_min_length(const code_t *code, schema_t *schema)
{
    printf("minLength: %zu\n", (size_t)code->number);

    return string_length(schema->node->string) >= (size_t)code->number;
}

static int eval_max_length(const code_t *code, schema_t *schema)
{
    printf("maxLength: %zu\n", (size_t)code->number);

    return string_length(schema->node->string) <= (size_t)code->number;
}

static int eval_min(const code_t *code, schema_t *schema)
{
    printf("min: %f\n", code->number);

    return schema->node->number >= code->number;
}

static int eval_max(const code_t *code, schema_t *schema)
{
    printf("max: %f\n", code->number);

    return schema->node->number <= code->number;
}

static int eval_multiple_of(const code_t *code, schema_t *schema)
{
    printf("multipleOf: %f\n", code->number);

    double quotient = schema->node->number / code->number;

    return fabs(quotient - round(quotient)) < 1e-9;
}

/******************************************************************************
 COMPILE CODE
******************************************************************************/

#ifdef DEBUG
    #define log(...) fprintf(stderr, __VA_ARGS__)
#else
    #define log(...) ((void)0)
#endif

typedef struct { unsigned keyword, index, type, size; } path_t;
typedef struct
{
    code_t *code;
    unsigned size, room;
    path_t path[SEXP_MAX_DEPTH];
} frame_t;

static code_t *frame_resize(frame_t *frame)
{
    if (frame->size == frame->room)
    {
        unsigned room = frame->room ? frame->room * 2 : 1;
        code_t *code = realloc(frame->code, sizeof(*code) * room);

        if (code == NULL)
        {
            return NULL;
        }
        frame->code = code;
        frame->room = room;
    }
    memset(&frame->code[frame->size], 0, sizeof(code_t));
    return &frame->code[frame->size++];
}

static int keyword_is_expected(const sexp_event_t *event, unsigned keyword)
{
    const frame_t *frame = event->data;
    const path_t *parent = event->depth ? &frame->path[event->depth - 1] : NULL;

    switch (keyword)
    {
        case KEYWORD_OBJECT:
        case KEYWORD_ARRAY:
        case KEYWORD_STRING:
        case KEYWORD_INTEGER:
        case KEYWORD_NUMBER:
        case KEYWORD_BOOLEAN:
        case KEYWORD_NULL:
            return parent != NULL ? parent->size ? 0
                : parent->keyword != KEYWORD_PROPERTY
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
        case KEYWORD_MULTIPLE_OF:
            return parent != NULL
                ? (parent->keyword == KEYWORD_INTEGER) ||
                  (parent->keyword == KEYWORD_NUMBER)
                : 0;
        default:
            return 0;
    }
}

static int expression_is_valid(const sexp_event_t *event)
{
    const frame_t *frame = event->data;
    const path_t *path = &frame->path[event->depth];

    switch (path->keyword)
    {
        case KEYWORD_PROPERTY:
            return path->type != SEXP_UNDEFINED
                    ? path->type == SEXP_STRING
                    : path->size == 0;
        case KEYWORD_PATTERN:
        case KEYWORD_FORMAT:
        case KEYWORD_MASK:
            return path->type == SEXP_STRING;
        case KEYWORD_MIN_LENGTH:
        case KEYWORD_MAX_LENGTH:
            return (path->type == SEXP_INTEGER) &&
                   (frame->code[path->index].number >= 0) &&
                   (frame->code[path->index].number <= -1u);
        case KEYWORD_MIN:
        case KEYWORD_MAX:
            return path[-1].keyword == KEYWORD_INTEGER
                    ? path->type == SEXP_INTEGER
                    : (path->type & SEXP_NUMBER) != 0;
        case KEYWORD_MULTIPLE_OF:
            return (path[-1].keyword == KEYWORD_INTEGER
                    ? path->type == SEXP_INTEGER
                    : (path->type & SEXP_NUMBER) != 0
                   ) && (frame->code[path->index].number > 0);
        default:
            return path->type == SEXP_UNDEFINED;
    }
}

static int push_symbol(const sexp_event_t *event)
{
    unsigned keyword = keyword_id(event->string);

    if (keyword == INVALID_KEYWORD)
    {
        log("Invalid keyword '%s'\n", event->string);
        return 0;
    }
    if (!keyword_is_expected(event, keyword))
    {
        log("Unexpected keyword '%s'\n", event->string); 
        return 0;
    }

    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth];

    path->keyword = keyword;
    path->index = frame->size;
    path->type = SEXP_UNDEFINED;
    path->size = 0;

    code_t *code = frame_resize(frame);

    if (code == NULL)
    {
        return 0;
    }
    if (event->depth && keyword_is_type(keyword))
    {
        path[-1].size++;
    }
    switch (keyword)
    {
        case KEYWORD_OBJECT:
            code->action = eval_object;
            break;
        case KEYWORD_ARRAY:
            code->action = eval_array;
            break;
        case KEYWORD_STRING:
            code->action = eval_string;
            break;
        case KEYWORD_INTEGER:
            code->action = eval_integer;
            break;
        case KEYWORD_NUMBER:
            code->action = eval_number;
            break;
        case KEYWORD_BOOLEAN:
            code->action = eval_boolean;
            break;
        case KEYWORD_NULL:
            code->action = eval_null;
            break;
        case KEYWORD_PROPERTY:
            code->action = eval_property;
            break;
        case KEYWORD_ITEM:
            code->action = eval_item;
            break;
        case KEYWORD_PATTERN:
            code->action = eval_pattern;
            break;
        case KEYWORD_FORMAT:
            code->action = eval_format;
            break;
        case KEYWORD_MASK:
            code->action = eval_mask;
            break;
        case KEYWORD_MIN_LENGTH:
            code->action = eval_min_length;
            break;
        case KEYWORD_MAX_LENGTH:
            code->action = eval_max_length;
            break;
        case KEYWORD_MIN:
            code->action = eval_min;
            break;
        case KEYWORD_MAX:
            code->action = eval_max;
            break;
        case KEYWORD_MULTIPLE_OF:
            code->action = eval_multiple_of;
            break;
        default:
            return 0;
    }
    return 1;
}

static int push_symbol_end(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth];
    code_t *code = &frame->code[path->index];

    if (!expression_is_valid(event))
    {
        log("Malformed expression '%s'\n", keywords[path->keyword]);
        return 0;
    }
    switch (path->keyword)
    {
        case KEYWORD_OBJECT:
            code->pair[0] = path->size;
            code = frame_resize(frame);
            if (code == NULL)
            {
                return 0;
            }
            code->action = eval_object_end;
            break;
        case KEYWORD_ARRAY:
            if (path->size != 1)
            {
                code->action = eval_tuple;
                code->number = path->size;
            }
            code = frame_resize(frame);
            if (code == NULL)
            {
                return 0;
            }
            if (path->size == 1)
            {
                code->action = eval_array_end;
                code->number = frame->size - path->index - 2;
            }
            else
            {
                code->action = eval_tuple_end;
            }
            break;
        case KEYWORD_PROPERTY:
            if ((path->type == SEXP_UNDEFINED) && (path->size == 0))
            {
                frame->code[path[-1].index].pair[1] = -1u;
                path[-1].size--;
                frame->size--;
            }
            break;
        case KEYWORD_STRING:
            code->pair[1] = -1u;
            break;
        case KEYWORD_MIN_LENGTH:
            frame->code[path[-1].index].pair[0] = (unsigned)code->number;
            frame->size--;
            break; 
        case KEYWORD_MAX_LENGTH:
            frame->code[path[-1].index].pair[1] = (unsigned)code->number;
            frame->size--;
            break; 
    }
    if (event->depth == 0)
    {
        return frame_resize(frame) != NULL;
    }
    return 1;
}

static int push_scalar(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *parent = &frame->path[event->depth - 1];
    code_t *code = &frame->code[parent->index];
    
    if (parent->type != SEXP_UNDEFINED)
    {
        event->type == SEXP_STRING
            ? log("Unexpected scalar '%s'\n", event->string)
            : log("Unexpected scalar '%g'\n", event->number);
        return 0;
    }
    if (event->type == SEXP_STRING)
    {
        code->string = event->string;
    }
    else
    {
        code->number = event->number;
    }
    parent->type = event->type;
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
        case SEXP_INTEGER:
        case SEXP_REAL:
            return push_scalar(event);
        default:
            log("Unexpected event: %u\n", event->type);
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
        free(frame.code);
        return NULL;
    }
    return frame.code;
}

int json_validate(const json_t *node, const void *code,
    json_validate_callback callback, void *data)
{
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
}

