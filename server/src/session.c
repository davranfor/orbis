/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/clib_math.h>
#include "session.h"

#define TOKEN_SIZE 64

/**
 * Parse cookie in the form:
 * Cookie: [third-party-cookie;] session=<int>:<int>:<hex 64 bytes> [third-party-cookie]
 * If 'session' is not found and '/api/login' is not found, then return 0 (Unauthorized)
 */
int session_parse(session_t *session, const char *path, char *str)
{
    if ((str = strstr(str, "\r\nCookie: ")))
    {
        str += 10;

        char *end = strchr(str, '\r');

        *end = '\0';
        if ((str = strstr(str, "session=")) && ((str[-1] == ' ') || (str[-1] == ';')))
        {
            str += 8;

            int data[2] = { 0 };

            for (int i = 0; i < 2; i++)
            {
                char *ptr;

                data[i] = (int)strtol(str, &ptr, 10);
                if ((*ptr != ':') || (data[i] <= 0))
                {
                    return -1;
                }
                str = ptr + 1;
            }
            if (end - str < TOKEN_SIZE)
            {
                return -1;
            }
            str[TOKEN_SIZE] = '\0';
            session->user = data[0];
            session->role = data[1];
            session->token = str;
            return 1;
        }
    }
    if (!strcmp(path, "POST /api/login"))
    {
        session->token = "";
        return 1;
    }
    return 0;
}

int session_create(int user, int role, const char *token, char *session)
{
    if (token[0] != '\0')
    {
        snprintf(session, SESSION_SIZE, "%d:%d:%s", user, role, token);
    }
    else
    {
        unsigned char bytes[(TOKEN_SIZE + 1) / 2];

        if (!rand_bytes(bytes, sizeof bytes))
        {
            return 0;
        }

        char *output = session + snprintf(session, SESSION_SIZE, "%d:%d:", user, role);

        for (size_t i = 0; i < sizeof bytes; i++)
        {
            snprintf(output + (i * 2), 3, "%02x", bytes[i]);
        }
        output[TOKEN_SIZE] = '\0';
    }
    return 1;
}

