/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef JSON_STRUCT_H
#define JSON_STRUCT_H

#include "json_header.h"

void *json_compile(char *);
int json_validate(const json_t *, const void *);

#endif

