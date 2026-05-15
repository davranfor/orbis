/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef JSON_VALIDATOR_H
#define JSON_VALIDATOR_H

#include "json_header.h"

typedef void (*json_validate_callback)(const json_t *, void *);

void *json_compile(char *);
int json_validate(const json_t *, const void *, json_validate_callback, void *);

#endif

