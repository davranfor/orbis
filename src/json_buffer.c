/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clib_math.h"
#include "clib_check.h"
#include "clib_unicode.h"
#include "json_private.h"
#include "json_buffer.h"

/**
 * The 'encoding' static global variable can take the following values:
 * - JSON_ASCII: Escapes control characters and non-ASCII characters.
 * - JSON_UTF8:  Escapes control characters only.
 * The default value is JSON_UTF8.
 */
static enum json_encoding encoding = JSON_UTF8;

enum json_encoding json_get_encoding(void)
{
    return encoding;
}

void json_set_encoding(enum json_encoding mode)
{
    encoding = mode;
}

/**
 * An IEEE 754 double needs at most 17 significant digits to round-trip
 * exactly. NUMBER_CHARS covers the worst case %g can print around them:
 * a '-' for the mantissa, the decimal point, 'e', a '-' for the exponent
 * and 3 exponent digits (doubles range up to 1e+308) — 17+1+1+1+1+3 = 24.
 */
#define MAX_DECIMALS 17
#define NUMBER_CHARS 24

static char *write_integer(buffer_t *buffer, double number)
{
    char str[NUMBER_CHARS + 1];

    snprintf(str, sizeof str, "%.0f", number);
    return buffer_write(buffer, str);
}

static char *write_real(buffer_t *buffer, double number)
{
    char str[NUMBER_CHARS + 1];
    int length = snprintf(str, sizeof str, "%.*g", MAX_DECIMALS, number);

    // Dot followed by trailing zeros are removed when %g is used
    if (str[strspn(str, "-0123456789")] == '\0')
    {
        memcpy(str + length, ".0", 3);
    }
    return buffer_write(buffer, str);
}

static char *write_string(buffer_t *buffer, const char *str)
{
    buffer_put(buffer, '"');

    const char *ptr = str;

    while (*str != '\0')
    {
        char esc = encode_esc(str);

        if (esc != '\0')
        {
            const char seq[] = { '\\', esc, '\0' };

            buffer_append(buffer, ptr, (size_t)(str - ptr));
            buffer_append(buffer, seq, 2);
            ptr = ++str;
        }
        else if (is_cntrl(*str) || ((encoding == JSON_ASCII) && !is_ascii(*str)))
        {
            char seq[sizeof("\\u0123")] = { '\0' };
            size_t length = encode_hex(str, seq);

            buffer_append(buffer, ptr, (size_t)(str - ptr));
            buffer_append(buffer, seq, 6);
            str += length;
            ptr = str;
        }
        else
        {
            str++;
        }
    }
    buffer_append(buffer, ptr, (size_t)(str - ptr));
    return buffer_put(buffer, '"');
}

/**
 * encode_node()/encode_edge()/encode_tree() are mutually recursive and
 * walk the tree in DFS pre-order. 'trailing_comma' is decided by the
 * caller (node->size > i + 1, i.e. "is there a next sibling?") because
 * JSON forbids a comma after the last element of an object or array.
 */
static int encode_node(buffer_t *buffer, const json_t *node,
    unsigned short depth, unsigned char indent,
    unsigned char trailing_comma)
{
    size_t spaces = depth * indent;

    if (spaces > 0)
    {
        buffer_repeat(buffer, ' ', spaces);
    }
    if (node->key != NULL)
    {
        write_string(buffer, node->key);
        buffer_write(buffer, indent == 0 ? ":" : ": ");
    }
    switch (node->type)
    {
        case JSON_OBJECT:
            buffer_put(buffer, '{');
            break;
        case JSON_ARRAY:
            buffer_put(buffer, '[');
            break;
        case JSON_STRING:
            write_string(buffer, node->string);
            break;
        case JSON_INTEGER:
            write_integer(buffer, node->number);
            break;
        case JSON_REAL:
            write_real(buffer, node->number);
            break;
        case JSON_TRUE:
            buffer_write(buffer, "true");
            break;
        case JSON_FALSE:
            buffer_write(buffer, "false");
            break;
        case JSON_NULL:
            buffer_write(buffer, "null");
            break;
    }
    if (node->size == 0)
    {
        switch (node->type)
        {
            case JSON_OBJECT:
                buffer_put(buffer, '}');
                break;
            case JSON_ARRAY:
                buffer_put(buffer, ']');
                break;
        }
        if (trailing_comma)
        {
            buffer_put(buffer, ',');
        }
    }
    if (indent > 0)
    {
        buffer_put(buffer, '\n');
    }
    return buffer->text != NULL;
}

static int encode_edge(buffer_t *buffer, const json_t *node,
    unsigned short depth, unsigned char indent,
    unsigned char trailing_comma)
{
    size_t spaces = depth * indent;

    if (spaces > 0)
    {
        buffer_repeat(buffer, ' ', spaces);
    }
    switch (node->type)
    {
        case JSON_OBJECT:
            buffer_put(buffer, '}');
            break;
        case JSON_ARRAY:
            buffer_put(buffer, ']');
            break;
    }
    if (trailing_comma)
    {
        buffer_put(buffer, ',');
    }
    if (indent > 0)
    {
        buffer_put(buffer, '\n');
    }
    return buffer->text != NULL;
}

#define MAX_INDENT 8

static int encode_tree(buffer_t *buffer, const json_t *node,
    unsigned short depth, unsigned char indent)
{
    for (unsigned i = 0; i < node->size; i++)
    {
        unsigned char more = node->size > i + 1;

        CHECK(encode_node(buffer, &node->child[i], depth, indent, more));
        if (node->child[i].size > 0)
        {
            CHECK(encode_tree(buffer, &node->child[i], depth + 1, indent));
            CHECK(encode_edge(buffer, &node->child[i], depth, indent, more));
        }
    }
    return 1;
}

/**
 * Encodes a node into a provided buffer.
 * The cast from 'const json_t *' to 'json_t *' is needed to pack the children.
 * If the passed node IS a property, add parent and grandparent: [{key: value}]
 * If the passed node IS NOT a property, add parent: [value]
 */
static char *buffer_encode(buffer_t *buffer, const json_t *node, size_t indent)
{
    if (node == NULL)
    {
        return NULL;
    }

    const json_t parent =
    {
        .child = json_cast(node),
        .type = node->key ? JSON_OBJECT : JSON_ARRAY,
        .size = 1
    };
    const json_t grandparent =
    {
         .child = json_cast(&parent),
         .type = JSON_ARRAY,
         .size = 1
    };

    if (indent > MAX_INDENT)
    {
        indent = MAX_INDENT;
    }
    if (node->key != NULL)
    {
        CHECK(encode_tree(buffer, &grandparent, 0, (unsigned char)indent));
    }
    else
    {
        CHECK(encode_tree(buffer, &parent, 0, (unsigned char)indent));
    }
    return buffer->text;
}

/* Serializes a JSON structure or a single node into a compact string */
char *json_encode(const json_t *node, size_t indent)
{
    buffer_t buffer = { 0 };

    return buffer_encode(&buffer, node, indent);
}

/* Serializes into a provided buffer */
char *json_buffer_encode(buffer_t *buffer, const json_t *node, size_t indent)
{
    if (buffer && buffer_encode(buffer, node, indent))
    {
        return buffer->text;
    }
    return NULL;
}

/* Serializes without indentation */
char *json_stringify(const json_t *node)
{
    buffer_t buffer = { 0 };

    return buffer_encode(&buffer, node, 0);
}

#define write_file(buffer, file) \
    (fwrite(buffer.text, 1, buffer.length, file) == buffer.length)

/* Serializes into a file */
int json_write(const json_t *node, FILE *file, size_t indent)
{
    int rc = 0;

    if (file != NULL)
    {
        buffer_t buffer = { 0 };

        if (buffer_encode(&buffer, node, indent))
        {
            rc = write_file(buffer, file);
        }
        free(buffer.text);
    }
    return rc;
}

/* Serializes into a file with a trailing newline */
int json_write_line(const json_t *node, FILE *file)
{
    int rc = 0;

    if (file != NULL)
    {
        buffer_t buffer = { 0 };

        if (buffer_encode(&buffer, node, 0) && buffer_put(&buffer, '\n'))
        {
            rc = write_file(buffer, file);
        }
        free(buffer.text);
    }
    return rc;
}

/* Serializes into a FILE given a path */
int json_write_file(const json_t *node, const char *path, size_t indent)
{
    FILE *file;
    int rc = 0;

    if ((node != NULL) && (path != NULL) && (file = fopen(path, "w")))
    {
        buffer_t buffer = { 0 };

        if (buffer_encode(&buffer, node, indent))
        {
            rc = write_file(buffer, file);
        }
        free(buffer.text);
        fclose(file);
    }
    return rc;
}

/* Serializes and sends the result to stdout (2 spaces) */
int json_print(const json_t *node)
{
    return json_write(node, stdout, 2);
}

/* Returns an encoded json string */
char *json_quote(const char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    buffer_t buffer = { 0 };

    return write_string(&buffer, str);
}

/* Encodes a json string into a provided buffer */
char *json_buffer_quote(buffer_t *buffer, const char *str)
{
    if ((buffer == NULL) || (str == NULL))
    {
        return NULL;
    }
    return write_string(buffer, str);
}

/* Returns an encoded json string from a number */
char *json_convert(double number, unsigned type)
{
    buffer_t buffer = { 0 };

    if ((type == JSON_INTEGER) && IS_SAFE_INTEGER(number))
    {
        return write_integer(&buffer, number);
    }
    else
    {
        return write_real(&buffer, number);
    }
}

/* Encodes a number as json string into a provided buffer */
char *json_buffer_convert(buffer_t *buffer, double number, unsigned type)
{
    if (buffer == NULL)
    {
        return NULL;
    }
    if ((type == JSON_INTEGER) && IS_SAFE_INTEGER(number))
    {
        return write_integer(buffer, number);
    }
    else
    {
        return write_real(buffer, number);
    }
}

