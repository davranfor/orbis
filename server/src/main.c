/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>
#include "config.h"
#include "loader.h"
#include "server.h"

static uint16_t port_number(const char *str)
{
    char *end;
    unsigned long result = strtoul(str, &end, 10);

    if ((result > 65535) || (end[strspn(end, " \f\n\r\t\v")] != '\0'))
    {
        return 0;
    }
    return (uint16_t)result;
}

int main(int argc, char *argv[])
{
    if ((argc == 2) && (strcmp(argv[1], "-h") == 0))
    {
        printf("Usage: %s [port]\n", argv[0]);
        return 0;
    }

    uint16_t port = argc > 1 ? port_number(argv[1]) : SERVER_PORT;

    if (port == 0)
    {
        fprintf(stderr, "Invalid port\n");
        exit(EXIT_FAILURE);
    }
    setlocale(LC_NUMERIC, "C");
    loader_load();
    printf("Server waiting on port %u\n\n", port);
    server_run(port);
    return 0;
}

