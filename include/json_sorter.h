/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef JSON_SORTER_H
#define JSON_SORTER_H

#include "json_header.h"

typedef int (*json_sort_callback)(const void *, const void *);

json_t *json_search(const json_t *, const void *, json_sort_callback);
void json_sort(json_t *, json_sort_callback);
void json_reverse(json_t *);

#endif

