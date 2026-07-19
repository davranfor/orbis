/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef SOLVER_H
#define SOLVER_H

#include <orbis/clib_buffer.h>
#include <orbis/json_header.h>
#include "session.h"

void solver_load(void);
void solver_reload(void);
const buffer_t *solver_handle(const char *, session_t *, const json_t *);

#endif

