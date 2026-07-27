/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef STATIC_H
#define STATIC_H

#include <orbis/clib_buffer.h>

void static_load(void);
void static_reload(void);
const buffer_t *static_bad_request(void);
const buffer_t *static_unauthorized(void);
const buffer_t *static_server_error(void);

#endif

