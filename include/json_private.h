/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef JSON_PRIVATE_H
#define JSON_PRIVATE_H

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

