/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#ifndef CONFIG_H
#define CONFIG_H

#define SERVER_PORT 8000

#define MAX_CLIENTS 64

#define BUFFER_SIZE 32768

#define HEADERS_MAX_LENGTH 4096

#define REQUEST_MAX_LENGTH (1024 * 1024 * 4)
#define REQUEST_MAX_PARAMS 16
#define REQUEST_MAX_FIELDS 64

#endif

