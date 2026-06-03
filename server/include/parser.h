/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef PARSER_H
#define PARSER_H

#include <orbis/clib_buffer.h>

int parser_status(const char *, size_t);
const buffer_t *parser_handle(char *);

#endif

