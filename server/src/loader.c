/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include "static.h"
#include "router.h"
#include "solver.h"
#include "loader.h"

void loader_load(void)
{
    static_load();
    router_load();
    solver_load();
}

void loader_reload(void)
{
    router_reload();
    solver_reload();
}

