/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef JSON_PRIVATE_H
#define JSON_PRIVATE_H

/**
 * child/string/number are mutually exclusive per node->type.
 * index/span only hold meaning transiently while json_decode()
 * (json_writer.c) rebuilds the tree from parser events; once decoding
 * finishes every container's slot holds 'child' and is never read as
 * index/span again.
 */
struct json
{
    char *key;
    union
    {
        struct json *child; char *string; double number;
        struct { unsigned index, span; };
    };
    unsigned type, size;
};

#endif

