/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include "router.h"
#include "static.h"
#include "solver.h"
#include "loader.h"

void loader_load(void)
{
    router_load();
    static_load();
    solver_load();
}

void loader_reload(void)
{
    router_reload();
    static_reload();
    solver_reload();
}

