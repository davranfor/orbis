/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef SOLVER_H
#define SOLVER_H

#include <orbis/clib_buffer.h>
#include <orbis/json_header.h>
#include "session.h"

void solver_load(void);
void solver_reload(void);
const buffer_t *solver_handle(const json_t *, const char *, session_t *);

#endif

