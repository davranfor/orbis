/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef CLIB_CHECK_H
#define CLIB_CHECK_H

/* return 0 if 'expr' fails */
#define CHECK(expr) do { if (!(expr)) return 0; } while (0)

#endif

