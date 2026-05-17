/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "clib_string.h"
#include "clib_regex.h"
#include "clib_match.h"
#include "sexp_parser.h"
#include "json_private.h"
#include "json_reader.h"
#include "json_validator.h"

#define KEYWORD(_)                          \
    _(KEYWORD_OBJECT,       "object")       \
    _(KEYWORD_TUPLE,        "tuple")        \
    _(KEYWORD_ARRAY,        "array")        \
    _(KEYWORD_STRING,       "string")       \
    _(KEYWORD_INTEGER,      "integer")      \
    _(KEYWORD_NUMBER,       "number")       \
    _(KEYWORD_BOOLEAN,      "boolean")      \
    _(KEYWORD_NULL,         "null")         \
    _(KEYWORD_PROPERTY,     "property")     \
    _(KEYWORD_OPTIONAL,     "optional")     \
    _(KEYWORD_NULLABLE,     "nullable")     \
    _(KEYWORD_UNIQUE_ITEMS, "uniqueItems")  \
    _(KEYWORD_MIN_ITEMS,    "minItems")     \
    _(KEYWORD_MAX_ITEMS,    "maxItems")     \
    _(KEYWORD_CONST,        "const")        \
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
        case KEYWORD_TUPLE:
        case KEYWORD_ARRAY:
        case KEYWORD_STRING:
        case KEYWORD_INTEGER:
        case KEYWORD_NUMBER:
        case KEYWORD_BOOLEAN:
        case KEYWORD_NULL:
            return 1;
        default:
            return 0;
    } 
}

/******************************************************************************
 EVAL CODE
******************************************************************************/

enum
{
    FLAG_OPTIONAL = 1,
    FLAG_NULLABLE = 2,
    FLAG_UNIQUE_ITEMS = 4,
    FLAG_ADDITIONAL_PROPERTIES = 8
};

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
    union
    {
        char *string;
        double number;
        struct { unsigned jump; unsigned short type, flags; };
        unsigned pair[2];
    };
} code_t;

static int raise_error(const schema_t *, const char *, ...)
    __attribute__((format(printf, 2, 3)));

static size_t write_key(char *key, char *path, size_t size)
{
    int length = snprintf(path, size, "/%s", key);

    return length < 0 ? 0 : (size_t)length;
}

static size_t write_index(unsigned index, char *path, size_t size)
{
    int length = snprintf(path, size, "/%u", index);

    return length < 0 ? 0 : (size_t)length;
}

static size_t write_node(const json_t *parent, const json_t *child,
     char *path, size_t size)
{
    if (child->key != NULL)
    {
        return write_key(child->key, path, size);
    }
    if (parent->size > 0)
    {
        return write_index((unsigned)(child - parent->child), path, size);
    }
    return 0;
}

static void write_path(const schema_t *schema, char *str, size_t size)
{
    size_t length = 0, max_length = size - 4;
    char *path = str;

    for (unsigned depth = 1; depth < schema->depth; depth++)
    {
        const json_t *parent = schema->path[depth - 1];
        const json_t *child = schema->path[depth];
        size_t bytes = write_node(parent, child, path, size);

        length += bytes;
        if (length > max_length)
        {
            length = string_truncate(str, max_length);
            snprintf(str + length, 4, "...");
            return;
        }
        path += bytes;
        size -= bytes;
    }
    if ((schema->depth > 0) &&
        (schema->item[schema->depth - 1] > 0) &&
        (schema->node->type & JSON_SCALAR))
    {
        const json_t *parent = schema->path[schema->depth - 1];
        unsigned index = schema->item[schema->depth - 1] - 1;

        if (index < parent->size)
        {
            const json_t *child = &parent->child[index];

            length += write_node(parent, child, path, size);
            if (length > max_length)
            {
                length = string_truncate(str, max_length);
                snprintf(str + length, 4, "...");
                return;
            }
        }
    }
}

static void truncate_rule(char *rule, size_t size)
{
    size_t length = string_truncate(rule, size - 4);

    snprintf(rule + length, 4, "...");
}

static int raise_error(const schema_t *schema, const char *fmt, ...)
{
    if (schema->callback == NULL)
    {
        return 0;
    }

    char path[256] = "/";

    write_path(schema, path, sizeof path);

    va_list args;

    va_start(args, fmt);

    char rule[256];
    int length = vsnprintf(rule, sizeof rule, fmt, args);

    if ((length > 0) && ((size_t)length >= sizeof rule))
    {
        truncate_rule(rule, sizeof rule);
    }
    va_end(args);

    const json_t node =
    {
        .child = (json_t [])
        {
            { .key = "path", .string = path, .type = JSON_STRING },
            { .key = "rule", .string = rule, .type = JSON_STRING },
        },
        .type = JSON_OBJECT,
        .size = 2
    };

    schema->callback(&node, schema->data);
    return 0;
}

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
    (void)code;
    if (schema->node->type != JSON_OBJECT)
    {
        return raise_error(schema, "type: object");
    }
    schema->path[schema->depth] = schema->node;
    schema->item[schema->depth] = 0;
    schema->depth++;
    return 1;
}

static int eval_object_end(const code_t *code, schema_t *schema)
{
    if (!(code->flags & FLAG_ADDITIONAL_PROPERTIES))
    {
        const json_t *object = schema->path[schema->depth - 1];

        if (object->size != schema->item[schema->depth - 1])
        {
            return raise_error(schema, "additionalProperties: false");
        }
    }   
    schema->node = schema->path[--schema->depth];
    return 1;
}

static int eval_tuple(const code_t *code, schema_t *schema)
{
    (void)code;
    if (schema->node->type != JSON_ARRAY)
    {
        return raise_error(schema, "type: array");
    }
    schema->path[schema->depth] = schema->node;
    schema->item[schema->depth] = 0;
    schema->depth++;
    return 1;
}

static int eval_tuple_end(const code_t *code, schema_t *schema)
{
    (void)code;

    const json_t *array = schema->path[schema->depth - 1];
    unsigned index = schema->item[schema->depth - 1];

    if (array->size != index)
    {
        schema->node = schema->path[schema->depth - 1];
        return raise_error(schema, "maxItems: %u", index);
    }
    schema->node = schema->path[--schema->depth];
    return 1;
}

static int eval_array(const code_t *code, schema_t *schema)
{
    if (schema->node->type != JSON_ARRAY)
    {
        return raise_error(schema, "type: array");
    }

    const unsigned *pair = code[-1].pair;

    if (schema->node->size < pair[0])
    {
        return raise_error(schema, "minItems: %u", pair[0]);
    }
    if (schema->node->size > pair[1])
    {
        return raise_error(schema, "maxItems: %u", pair[1]);
    }
    if ((code->flags & FLAG_UNIQUE_ITEMS) &&
        !json_unique_items(schema->node))
    {
        return raise_error(schema, "uniqueItems: true");
    }
    if (code->jump == 0)
    {
        return 2;
    }
    if (schema->node->size == 0)
    {
        return (int)code->jump + 2;
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
    return -(int)code->jump;
}

static int eval_string(const code_t *code, schema_t *schema)
{
    (void)code;
    if (schema->node->type == JSON_STRING)
    {
        return 1;
    }
    return raise_error(schema, "type: string");
}

static int eval_integer(const code_t *code, schema_t *schema)
{
    (void)code;
    if (schema->node->type == JSON_INTEGER)
    {
        return 1;
    }
    return raise_error(schema, "type: integer");
}

static int eval_number(const code_t *code, schema_t *schema)
{
    (void)code;
    if (schema->node->type & JSON_NUMBER)
    {
        return 1;
    }
    return raise_error(schema, "type: number");
}

static int eval_boolean(const code_t *code, schema_t *schema)
{
    (void)code;
    if (schema->node->type & JSON_BOOLEAN)
    {
        return 1;
    }
    return raise_error(schema, "type: boolean");
}

static int eval_null(const code_t *code, schema_t *schema)
{
    (void)code;
    if (schema->node->type == JSON_NULL)
    {
        return 1;
    }
    return raise_error(schema, "type: null");
}

static int eval_property(const code_t *code, schema_t *schema)
{
    const json_t *object = schema->path[schema->depth - 1]; 

    for (unsigned index = 0; index < object->size; index++)
    {
        if (!strcmp(object->child[index].key, code->string))
        {
            schema->node = &object->child[index];
            schema->item[schema->depth - 1]++;
            if ((code[-1].flags & FLAG_NULLABLE) &&
                (schema->node->type == JSON_NULL))
            {
                return (int)code[-1].jump;
            }
            return 1;
        }
    }
    if (code[-1].flags & FLAG_OPTIONAL)
    {
        return (int)code[-1].jump;
    }
    schema->node = schema->path[schema->depth - 1];
    return raise_error(schema, "required: %s", code->string);
}

static int eval_item(const code_t *code, schema_t *schema)
{
    const json_t *array = schema->path[schema->depth - 1];
    unsigned *index = &schema->item[schema->depth - 1];

    if (array->size > *index)
    {
        schema->node = &array->child[(*index)++];
        switch (code->type)
        {
            case KEYWORD_OBJECT:
                return eval_object(code, schema);
            case KEYWORD_TUPLE:
                return eval_tuple(code, schema);
            case KEYWORD_ARRAY:
                return eval_array(code, schema);
            case KEYWORD_STRING:
                return eval_string(code, schema);
            case KEYWORD_INTEGER:
                return eval_integer(code, schema);
            case KEYWORD_NUMBER:
                return eval_number(code, schema);
            case KEYWORD_BOOLEAN:
                return eval_boolean(code, schema);
            case KEYWORD_NULL:
                return eval_null(code, schema);
        }
    }
    schema->node = schema->path[schema->depth - 1];
    return raise_error(schema, "minItems: %u", *index + 1);
}

static int eval_const(const code_t *code, schema_t *schema)
{
    if (schema->node->type == JSON_STRING)
    {
        return strcmp(schema->node->string, code->string)
            ? raise_error(schema, "const: %s", code->string)
            : 1;
    }
    if (schema->node->number != code->number)
    {
        return schema->node->type == JSON_INTEGER
            ? raise_error(schema, "const: %.0f", code->number)
            : raise_error(schema, "const: %.17g", code->number);
    }
    return 1;
}

static int eval_pattern(const code_t *code, schema_t *schema)
{
    if (test_regex(schema->node->string, code->string))
    {
        return 1;
    }
    return raise_error(schema, "pattern: %s", code->string);
}

static int eval_format(const code_t *code, schema_t *schema)
{
    if (test_match(schema->node->string, code->string))
    {
        return 1;
    }
    return raise_error(schema, "format: %s", code->string);
}

static int eval_mask(const code_t *code, schema_t *schema)
{
    if (test_mask(schema->node->string, code->string))
    {
        return 1;
    }
    return raise_error(schema, "mask: %s", code->string);
}

static int eval_min_length(const code_t *code, schema_t *schema)
{
    if (string_length(schema->node->string) >= (size_t)code->number)
    {
        return 1;
    }
    return raise_error(schema, "minLength: %.0f", code->number);
}

static int eval_max_length(const code_t *code, schema_t *schema)
{
    if (string_length(schema->node->string) <= (size_t)code->number)
    {
        return 1;
    }
    return raise_error(schema, "maxLength: %.0f", code->number);
}

static int eval_min(const code_t *code, schema_t *schema)
{
    if (schema->node->number >= code->number)
    {
        return 1;
    }
    return schema->node->type == JSON_INTEGER
        ? raise_error(schema, "min: %.0f", code->number)
        : raise_error(schema, "min: %.17g", code->number);
}

static int eval_max(const code_t *code, schema_t *schema)
{
    if (schema->node->number <= code->number)
    {
        return 1;
    }
    return schema->node->type == JSON_INTEGER
        ? raise_error(schema, "max: %.0f", code->number)
        : raise_error(schema, "max: %.17g", code->number);
}

static int eval_multiple_of(const code_t *code, schema_t *schema)
{
    double quotient = schema->node->number / code->number;

    if (fabs(quotient - round(quotient)) < 1e-9)
    {
        return 1;
    }
    return schema->node->type == JSON_INTEGER
        ? raise_error(schema, "multipleOf: %.0f", code->number)
        : raise_error(schema, "multipleOf: %.17g", code->number);
}

static int eval_meta(const code_t *code, schema_t *schema)
{
    (void)code;
    (void)schema;
    return 1;
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
    const path_t *parent = event->depth
        ? &frame->path[event->depth - 1]
        : NULL;

    switch (keyword)
    {
        case KEYWORD_OBJECT:
        case KEYWORD_TUPLE:
        case KEYWORD_ARRAY:
        case KEYWORD_STRING:
        case KEYWORD_INTEGER:
        case KEYWORD_NUMBER:
        case KEYWORD_BOOLEAN:
        case KEYWORD_NULL:
            if (parent == NULL)
            {
                return 1;
            }
            switch (parent->keyword)
            {
                case KEYWORD_PROPERTY:
                    return parent->type == SEXP_STRING;
                case KEYWORD_ARRAY:
                    return parent->size == 0;
                case KEYWORD_TUPLE:
                    return 1;
            }
            return 0;
        case KEYWORD_PROPERTY:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_OBJECT);
        case KEYWORD_OPTIONAL:
        case KEYWORD_NULLABLE:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_PROPERTY);
        case KEYWORD_UNIQUE_ITEMS:
        case KEYWORD_MIN_ITEMS:
        case KEYWORD_MAX_ITEMS:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_ARRAY);
        case KEYWORD_CONST:
            return parent != NULL
                ? (parent->keyword == KEYWORD_STRING)  ||
                  (parent->keyword == KEYWORD_INTEGER) ||
                  (parent->keyword == KEYWORD_NUMBER)
                : 0;
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
        case KEYWORD_MIN_ITEMS:
        case KEYWORD_MAX_ITEMS:
        case KEYWORD_MIN_LENGTH:
        case KEYWORD_MAX_LENGTH:
            return (path->type == SEXP_INTEGER) &&
                   (frame->code[path->index].number >= 0);
        case KEYWORD_CONST:
            return path[-1].keyword == KEYWORD_STRING
                    ? path->type == SEXP_STRING
                    : path[-1].keyword == KEYWORD_INTEGER
                        ? path->type == SEXP_INTEGER
                        : (path->type & SEXP_NUMBER) != 0;
        case KEYWORD_PATTERN:
        case KEYWORD_FORMAT:
        case KEYWORD_MASK:
            return path->type == SEXP_STRING;
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

static int code_set_action(code_t *code, unsigned keyword)
{
    switch (keyword)
    {
        case KEYWORD_OBJECT:
            code->action = eval_object;
            return 1;
        case KEYWORD_TUPLE:
            code->action = eval_tuple;
            return 1;
        case KEYWORD_ARRAY:
            code->action = eval_array;
            return 1;
        case KEYWORD_STRING:
            code->action = eval_string;
            return 1;
        case KEYWORD_INTEGER:
            code->action = eval_integer;
            return 1;
        case KEYWORD_NUMBER:
            code->action = eval_number;
            return 1;
        case KEYWORD_BOOLEAN:
            code->action = eval_boolean;
            return 1;
        case KEYWORD_NULL:
            code->action = eval_null;
            return 1;
        case KEYWORD_PROPERTY:
            code->action = eval_property;
            return 1;
        case KEYWORD_CONST:
            code->action = eval_const;
            return 1;
        case KEYWORD_PATTERN:
            code->action = eval_pattern;
            return 1;
        case KEYWORD_FORMAT:
            code->action = eval_format;
            return 1;
        case KEYWORD_MASK:
            code->action = eval_mask;
            return 1;
        case KEYWORD_MIN_LENGTH:
            code->action = eval_min_length;
            return 1;
        case KEYWORD_MAX_LENGTH:
            code->action = eval_max_length;
            return 1;
        case KEYWORD_MIN:
            code->action = eval_min;
            return 1;
        case KEYWORD_MAX:
            code->action = eval_max;
            return 1;
        case KEYWORD_MULTIPLE_OF:
            code->action = eval_multiple_of;
            return 1;
        case KEYWORD_OPTIONAL:
        case KEYWORD_NULLABLE:
        case KEYWORD_UNIQUE_ITEMS:
        case KEYWORD_MIN_ITEMS:
        case KEYWORD_MAX_ITEMS:
            return 1;
        default:
            return 0;
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
    switch (keyword)
    {
        case KEYWORD_ARRAY:
            path->index++;
            code->pair[0] = 0;
            code->pair[1] = -1u;
            code->action = eval_meta;
            code = frame_resize(frame);
            if (code == NULL)
            {
                return 0;
            }
            break;
        case KEYWORD_PROPERTY:
            path->index++;
            code->action = eval_meta;
            code = frame_resize(frame);
            if (code == NULL)
            {
                return 0;
            }
            break;
    }
    if (event->depth && keyword_is_type(keyword)) 
    {
        path[-1].size++;
        if (path[-1].keyword != KEYWORD_PROPERTY)
        {
            code->type = (unsigned short)keyword;
            code->action = eval_item;
            return 1;
        }
    }
    return code_set_action(code, keyword);
}

static int push_reduce(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *path = &frame->path[event->depth];

    if (!expression_is_valid(event))
    {
        log("Malformed expression '%s'\n", keywords[path->keyword]);
        return 0;
    }

    code_t *code;

    switch (path->keyword)
    {
        case KEYWORD_OBJECT:
            code = frame_resize(frame);
            if (code == NULL)
            {
                return 0;
            }
            code->flags = frame->code[path->index].flags;
            code->action = eval_object_end;
            break;
        case KEYWORD_TUPLE:
            code = frame_resize(frame);
            if (code == NULL)
            {
                return 0;
            }
            code->action = eval_tuple_end;
            break;
        case KEYWORD_ARRAY:
            code = frame_resize(frame);
            if (code == NULL)
            {
                return 0;
            }
            code->action = eval_array_end;
            code->jump = frame->size - path->index - 2;
            frame->code[path->index].jump = code->jump;
            break;
        case KEYWORD_PROPERTY:
            if (path->type == SEXP_UNDEFINED)
            {
                code = &frame->code[path[-1].index];
                code->flags |= FLAG_ADDITIONAL_PROPERTIES;
                frame->size -= 2;
            }
            else
            {
                code = &frame->code[path->index - 1];
                code->jump = frame->size - path->index;
            }
            break;
        case KEYWORD_OPTIONAL:
            code = &frame->code[path[-1].index - 1];
            code->flags |= FLAG_OPTIONAL;
            frame->size--;
            break;
        case KEYWORD_NULLABLE:
            code = &frame->code[path[-1].index - 1];
            code->flags |= FLAG_NULLABLE;
            frame->size--;
            break;
        case KEYWORD_UNIQUE_ITEMS:
            code = &frame->code[path[-1].index];
            code->flags |= FLAG_UNIQUE_ITEMS;
            frame->size--;
            break;
        case KEYWORD_MIN_ITEMS:
            code = &frame->code[path->index];
            frame->code[path[-1].index - 1].pair[0] = (unsigned)code->number;
            frame->size--;
            break;
        case KEYWORD_MAX_ITEMS:
            code = &frame->code[path->index];
            frame->code[path[-1].index - 1].pair[1] = (unsigned)code->number;
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
    
    if (parent->type != SEXP_UNDEFINED)
    {
        event->type == SEXP_STRING
            ? log("Unexpected scalar '%s'\n", event->string)
            : log("Unexpected scalar '%g'\n", event->number);
        return 0;
    }

    code_t *code = &frame->code[parent->index];

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
        case SEXP_REDUCE:
            return push_reduce(event);
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

