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
#include "clib_check.h"
#include "clib_string.h"
#include "clib_regex.h"
#include "clib_match.h"
#include "sexp_parser.h"
#include "json_private.h"
#include "json_reader.h"
#include "json_validator.h"

/*
---------------------------------------------------------------------
Schema DSL: S-expressions compiled to bytecode
---------------------------------------------------------------------
A schema is written as S-expressions (see sexp_parser.c) and turned
by json_compile() into a flat, contiguous array of code_t — no tree,
no heap-allocated nodes, one malloc'd/realloc'd array for the whole
schema. json_validate() then runs that array against a json_t tree
with eval_code(), a tiny bytecode interpreter (see its comment below
for the part that makes it one).

This file has two halves:

- EVAL CODE: the runtime. One eval_*() function per opcode, each one
  a code_t->action.

- COMPILE CODE: the compiler. push_symbol()/push_reduce()/push_scalar()
  are the sexp_parse() callback (SEXP_SYMBOL/SEXP_REDUCE/scalar events),
  driving a small compile-time stack (frame_t.path[], indexed by sexp
  nesting depth) that (1) rejects keywords that can't appear where
  they were written (keyword_is_expected(), expression_is_valid()) and
  (2) backpatches jump offsets — e.g. "optional"/"nullable" are only
  known once the whole property expression has been reduced, so they
  patch the code_t emitted earlier for KEYWORD_PROPERTY.
---------------------------------------------------------------------
*/

#define KEYWORD(_)                                  \
    _(KEYWORD_OBJECT,           "object")           \
    _(KEYWORD_TUPLE,            "tuple")            \
    _(KEYWORD_ARRAY,            "array")            \
    _(KEYWORD_STRING,           "string")           \
    _(KEYWORD_INTEGER,          "integer")          \
    _(KEYWORD_NUMBER,           "number")           \
    _(KEYWORD_BOOLEAN,          "boolean")          \
    _(KEYWORD_NULL,             "null")             \
    _(KEYWORD_ANY,              "any")              \
    _(KEYWORD_PROPERTY,         "property")         \
    _(KEYWORD_MIN_PROPERTIES,   "minProperties")    \
    _(KEYWORD_MAX_PROPERTIES,   "maxProperties")    \
    _(KEYWORD_OPTIONAL,         "optional")         \
    _(KEYWORD_NULLABLE,         "nullable")         \
    _(KEYWORD_ETCETERA,         "etc")              \
    _(KEYWORD_UNIQUE_ITEMS,     "uniqueItems")      \
    _(KEYWORD_MIN_ITEMS,        "minItems")         \
    _(KEYWORD_MAX_ITEMS,        "maxItems")         \
    _(KEYWORD_CONST,            "const")            \
    _(KEYWORD_ENUM,             "enum")             \
    _(KEYWORD_PATTERN,          "pattern")          \
    _(KEYWORD_FORMAT,           "format")           \
    _(KEYWORD_MASK,             "mask")             \
    _(KEYWORD_MIN_LENGTH,       "minLength")        \
    _(KEYWORD_MAX_LENGTH,       "maxLength")        \
    _(KEYWORD_MIN,              "min")              \
    _(KEYWORD_MAX,              "max")              \
    _(KEYWORD_MULTIPLE_OF,      "multipleOf")

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
    FLAG_ETCETERA = 4,
    FLAG_UNIQUE_ITEMS = 8,
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

/**
 * encode_key()/write_key()/write_index()/write_node()/write_path()
 * rebuild the failing node's JSON Pointer (RFC 6901, see json_pointer.c)
 * on the fly from schema->path[], escaping '~' and '/' as they go, into
 * a fixed 256-byte stack buffer (raise_error()'s 'path'). Truncated
 * with "..." if it would overflow rather than growing — the schema
 * stack doesn't touch the heap even to report an error.
 */
static size_t encode_key(char *key, size_t size)
{
    size_t length = strlen(key);

    for (size_t index = 0; ; index++)
    {
        index = strcspn(key + index, "~/") + index;
        if (key[index] == '\0')
        {
            break;
        }
        memmove(key + index + 1, key + index, length - index);
        memmove(key + index, key[index] == '~' ? "~0" : "~1", 2);
        if (length + 1 < size)
        {
            length++;
        }
        key[length] = '\0';
    }
    return length;
}

static size_t write_key(const char *key, char *path, size_t size)
{
    int length = snprintf(path, size, "/%s", key);

    return length < 0 ? 0 : encode_key(path + 1, size - 1) + 1;
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
    if (schema->depth > 0)
    {
        const json_t *parent = schema->path[schema->depth - 1];
        const json_t *child = schema->node;

        if (parent != child)
        {
            length += write_node(parent, child, path, size);
            if (length > max_length)
            {
                length = string_truncate(str, max_length);
                snprintf(str + length, 4, "...");
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

/**
 * The bytecode interpreter: each action's return value IS the number
 * of code_t slots to advance by, not a plain boolean. 0 means
 * validation failed (already reported through raise_error()) and
 * stops the run; 1 means "move to the next instruction"; anything
 * else is a forward or backward jump — e.g. eval_array_end() returns
 * a negative offset to loop back over remaining array items, and
 * eval_property()/eval_enum() jump forward to skip code that doesn't
 * apply (a missing optional property, an already-matched enum value).
 */
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
    if ((code->flags & FLAG_ETCETERA) == 0)
    {
        const json_t *object = schema->path[schema->depth - 1];

        if (object->size != schema->item[schema->depth - 1])
        {
            schema->node = object;
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
    if ((code->flags & FLAG_ETCETERA) == 0)
    {
        const json_t *array = schema->path[schema->depth - 1];

        if (array->size != schema->item[schema->depth - 1])
        {
            schema->node = array;
            return raise_error(schema, "additionalItems: false");
        }
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
    if ((code->jump == 0) || (schema->node->size == 0))
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
    schema->node = object;
    return raise_error(schema, "required: %s", code->string);
}

static int eval_min_properties(const code_t *code, schema_t *schema)
{
    const json_t *object = schema->path[schema->depth - 1]; 

    if (object->size >= (size_t)code->number)
    {
        return 1;
    }
    schema->node = object;
    return raise_error(schema, "minProperties: %.0f", code->number);
}

static int eval_max_properties(const code_t *code, schema_t *schema)
{
    const json_t *object = schema->path[schema->depth - 1]; 

    if (object->size <= (size_t)code->number)
    {
        return 1;
    }
    schema->node = object;
    return raise_error(schema, "maxProperties: %.0f", code->number);
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
    schema->node = array;
    return raise_error(schema, "minItems: %u", *index + 1);
}

static int eval_min_items(const code_t *code, schema_t *schema)
{
    const json_t *array = schema->path[schema->depth - 1]; 

    if (array->size >= (size_t)code->number)
    {
        return 1;
    }
    schema->node = array;
    return raise_error(schema, "minItems: %.0f", code->number);
}

static int eval_max_items(const code_t *code, schema_t *schema)
{
    const json_t *array = schema->path[schema->depth - 1]; 

    if (array->size <= (size_t)code->number)
    {
        return 1;
    }
    schema->node = array;
    return raise_error(schema, "maxItems: %.0f", code->number);
}

static int eval_const(const code_t *code, schema_t *schema)
{
    if (schema->node->type == JSON_STRING)
    {
        if (schema->node->string[0] == '\0')
        {
            return 1;
        }
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

static int eval_enum(const code_t *code, schema_t *schema)
{
    if (schema->node->type == JSON_STRING)
    {
        if (schema->node->string[0] == '\0')
        {
            return (int)code->jump;
        }
        for (unsigned i = 1; i < code->jump; i++)
        {
            if (!strcmp(schema->node->string, code[i].string))
            {
                return (int)code->jump;
            }
        }
        return raise_error(schema, "enum: [%s, ...]", code[1].string);
    }
    else
    {
        for (unsigned i = 1; i < code->jump; i++)
        {
            if (schema->node->number == code[i].number)
            {
                return (int)code->jump;
            }
        }
        return schema->node->type == JSON_INTEGER
            ? raise_error(schema, "enum: [%.0f, ...]", code[1].number)
            : raise_error(schema, "enum: [%.17g, ...]", code[1].number);
    }
}

static int eval_pattern(const code_t *code, schema_t *schema)
{
    if (schema->node->string[0] == '\0')
    {
        return 1;
    }
    if (test_regex(schema->node->string, code->string))
    {
        return 1;
    }
    return raise_error(schema, "pattern: %s", code->string);
}

static int eval_format(const code_t *code, schema_t *schema)
{
    if (schema->node->string[0] == '\0')
    {
        return 1;
    }
    if (test_match(schema->node->string, code->string))
    {
        return 1;
    }
    return raise_error(schema, "format: %s", code->string);
}

static int eval_mask(const code_t *code, schema_t *schema)
{
    if (schema->node->string[0] == '\0')
    {
        return 1;
    }
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

/**
 * One path_t per sexp nesting level (frame_t.path[event->depth]):
 * 'keyword' is the enclosing keyword, 'index' the code_t slot it
 * emitted (for later backpatching), 'type'/'size' track what's been
 * seen so far inside it (single scalar type allowed, item count...).
 * Discarded once its expression is reduced (SEXP_REDUCE) — this is
 * compile-time-only state, none of it survives into code_t.
 */
typedef struct { unsigned keyword, index, type, size; } path_t;
typedef struct
{
    code_t *code;
    unsigned size, room;
    path_t path[SEXP_MAX_DEPTH];
} frame_t;

static code_t *code_resize(frame_t *frame)
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
        case KEYWORD_ANY:
            code->action = eval_meta;
            return 1;
        case KEYWORD_PROPERTY:
            code->action = eval_property;
            return 1;
        case KEYWORD_MIN_PROPERTIES:
            code->action = eval_min_properties;
            return 1;
        case KEYWORD_MAX_PROPERTIES:
            code->action = eval_max_properties;
            return 1;
        case KEYWORD_MIN_ITEMS:
            code->action = eval_min_items;
            return 1;
        case KEYWORD_MAX_ITEMS:
            code->action = eval_max_items;
            return 1;
        case KEYWORD_CONST:
            code->action = eval_const;
            return 1;
        case KEYWORD_ENUM:
            code->action = eval_enum;
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
        case KEYWORD_ETCETERA:
        case KEYWORD_UNIQUE_ITEMS:
            return 1;
        default:
            return 0;
    }
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
        case KEYWORD_ANY:
            return parent == NULL;
        case KEYWORD_PROPERTY:
        case KEYWORD_MIN_PROPERTIES:
        case KEYWORD_MAX_PROPERTIES:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_OBJECT);
        case KEYWORD_OPTIONAL:
        case KEYWORD_NULLABLE:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_PROPERTY);
        case KEYWORD_ETCETERA:
            return parent != NULL
                ? (parent->keyword == KEYWORD_OBJECT) ||
                  (parent->keyword == KEYWORD_TUPLE)
                : 0;
        case KEYWORD_UNIQUE_ITEMS:
            return (parent != NULL) &&
                   (parent->keyword == KEYWORD_ARRAY);
        case KEYWORD_MIN_ITEMS:
        case KEYWORD_MAX_ITEMS:
            return parent != NULL
                ? (parent->keyword == KEYWORD_TUPLE) ||
                  (parent->keyword == KEYWORD_ARRAY)
                : 0;
        case KEYWORD_CONST:
        case KEYWORD_ENUM:
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

static int scalar_is_expected(const path_t *parent, unsigned type)
{
    if (parent->type == SEXP_UNDEFINED)
    {
        return 1;
    }
    return (parent->keyword == KEYWORD_ENUM) &&
           (parent->type == type);
}

static int expression_is_valid(const sexp_event_t *event)
{
    const frame_t *frame = event->data;
    const path_t *path = &frame->path[event->depth];

    switch (path->keyword)
    {
        case KEYWORD_PROPERTY:
        case KEYWORD_PATTERN:
        case KEYWORD_FORMAT:
        case KEYWORD_MASK:
            return path->type == SEXP_STRING;
        case KEYWORD_MIN_PROPERTIES:
        case KEYWORD_MAX_PROPERTIES:
        case KEYWORD_MIN_ITEMS:
        case KEYWORD_MAX_ITEMS:
        case KEYWORD_MIN_LENGTH:
        case KEYWORD_MAX_LENGTH:
            return (path->type == SEXP_INTEGER) &&
                   (frame->code[path->index].number >= 0);
        case KEYWORD_CONST:
        case KEYWORD_ENUM:
            return path[-1].keyword == KEYWORD_STRING
                    ? path->type == SEXP_STRING
                    : path[-1].keyword == KEYWORD_INTEGER
                        ? path->type == SEXP_INTEGER
                        : (path->type & SEXP_NUMBER) != 0;
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

    code_t *code;

    CHECK(code = code_resize(frame));
    /**
     * ARRAY and PROPERTY are the two keywords whose eval_*() needs more
     * state than a single code_t carries, so they get an extra "header"
     * slot right before their real action slot: a no-op (eval_meta) that
     * only exists to hold data. path->index is bumped to point past it,
     * at the action slot, so every other reference to path->index (here
     * and in push_reduce()) lands on the action; the action then reaches
     * code[-1] to read/patch its own header — eval_array() for pair
     * (min/maxItems), eval_property() for flags (optional/nullable) and
     * jump (how far to skip when the property doesn't apply).
     */
    switch (keyword)
    {
        case KEYWORD_ARRAY:
            path->index++;
            code->pair[0] = 0;
            code->pair[1] = -1u;
            code->action = eval_meta;
            CHECK(code = code_resize(frame));
            break;
        case KEYWORD_PROPERTY:
            path->index++;
            code->action = eval_meta;
            CHECK(code = code_resize(frame));
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
            CHECK(code = code_resize(frame));
            if (path->size == 0)
            {
                code->flags = FLAG_ETCETERA;
            }
            code->flags |= frame->code[path->index].flags;
            code->action = eval_object_end;
            break;
        case KEYWORD_TUPLE:
            CHECK(code = code_resize(frame));
            if (path->size == 0)
            {
                code->flags = FLAG_ETCETERA;
            }
            code->flags |= frame->code[path->index].flags;
            code->action = eval_tuple_end;
            break;
        case KEYWORD_ARRAY:
            CHECK(code = code_resize(frame));
            code->action = eval_array_end;
            code->jump = frame->size - path->index - 2;
            frame->code[path->index].jump = code->jump;
            break;
        case KEYWORD_PROPERTY:
            code = &frame->code[path->index - 1];
            code->jump = frame->size - path->index;
            path[-1].size++;
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
        case KEYWORD_ETCETERA:
            code = &frame->code[path[-1].index];
            code->flags |= FLAG_ETCETERA;
            frame->size--;
            break;
        case KEYWORD_UNIQUE_ITEMS:
            code = &frame->code[path[-1].index];
            code->flags |= FLAG_UNIQUE_ITEMS;
            frame->size--;
            break;
        case KEYWORD_MIN_ITEMS:
            if (path[-1].keyword == KEYWORD_ARRAY)
            {
                code = &frame->code[path->index];
                frame->code[path[-1].index - 1].pair[0] = (unsigned)code->number;
                frame->size--;
            }
            break;
        case KEYWORD_MAX_ITEMS:
            if (path[-1].keyword == KEYWORD_ARRAY)
            {
                code = &frame->code[path->index];
                frame->code[path[-1].index - 1].pair[1] = (unsigned)code->number;
                frame->size--;
            }
            break;
        case KEYWORD_ENUM:
            code = &frame->code[path->index];
            code->jump = frame->size - path->index;
            break;
    }
    if (event->depth > 0)
    {
        return 1;
    }
    return code_resize(frame) != NULL;
}

static int push_scalar(const sexp_event_t *event)
{
    frame_t *frame = event->data;
    path_t *parent = &frame->path[event->depth - 1];
  
    if (!scalar_is_expected(parent, event->type))
    {
        event->type == SEXP_STRING
            ? log("Unexpected scalar '%s'\n", event->string)
            : log("Unexpected scalar '%g'\n", event->number);
        return 0;
    }

    code_t *code = &frame->code[parent->index];

    if (parent->keyword == KEYWORD_ENUM)
    {
        CHECK(code = code_resize(frame));
        code->action = eval_meta;
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

