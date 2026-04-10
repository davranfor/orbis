/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef JSON_HEADER_H
#define JSON_HEADER_H

#define JSON_MAX_DEPTH 32

enum
{
    JSON_UNDEFINED = 0,
    JSON_OBJECT = 1,
    JSON_ARRAY = 2,
    JSON_STRING = 4,
    JSON_INTEGER = 8,
    JSON_REAL = 16,
    JSON_TRUE = 32,
    JSON_FALSE = 64,
    JSON_NULL = 128,
    JSON_END_OBJECT = 256,
    JSON_END_ARRAY = 512,
};

enum
{
    JSON_NUMBER = JSON_INTEGER | JSON_REAL,
    JSON_BOOLEAN = JSON_TRUE | JSON_FALSE,
    JSON_SCALAR = JSON_STRING | JSON_NUMBER | JSON_BOOLEAN | JSON_NULL,
    JSON_ITERABLE = JSON_OBJECT | JSON_ARRAY,
    JSON_END_ITERABLE = JSON_END_OBJECT | JSON_END_ARRAY,
};

typedef struct json json_t;

static inline json_t *json_cast(const json_t *node)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    return (json_t *)node;
#pragma GCC diagnostic pop
}

#endif

