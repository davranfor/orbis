/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/clib_math.h>
#include "session.h"

/**
 * Parse cookie in the form:
 * Cookie: [third-party-cookie;] session=<int>:<int>:<hex 64 bytes>
 */
int session_parse(session_t *session, const char *path, char *str)
{
    if (!strcmp(path, "POST /api/login"))
    {
        return 1;
    }
    if ((str = strstr(str, "\r\nCookie:")))
    {
        str += 9;

        char *end = strchr(str, '\r');

        *end = '\0';
        if ((str = strstr(str, " session=")))
        {
            str += 9;

            int num[2] = { 0 };

            for (int i = 0; i < 2; i++)
            {
                char *ptr;

                num[i] = (int)strtol(str, &ptr, 10);
                if ((*ptr != ':') || (num[i] <= 0))
                {
                    return 0;
                }
                str = ptr + 1;
            }
            if (end - str < TOKEN_MAX_LENGTH)
            {
                return 0;
            }
            session->user = num[0];
            session->role = num[1];
            memcpy(session->token, str, TOKEN_MAX_LENGTH);
            return 1;
        }
    }
    return 0;
}

const char *session_build(session_t *session, int user, int role,
    const char *token, int max_age)
{
    session->user = user;
    session->role = role;
    if ((token != NULL) && (token[0] != '\0'))
    {
        snprintf(session->token, TOKEN_MAX_LENGTH + 1, "%s", token);
    }
    else
    {
        unsigned char bytes[(TOKEN_MAX_LENGTH + 1) / 2];

        if (!rand_bytes(bytes, sizeof bytes))
        {
            return NULL;
        }
        for (size_t i = 0; i < sizeof bytes; i++)
        {
            snprintf(session->token + (i * 2), 3, "%02x", bytes[i]);
        }
    }
#ifdef ALLOW_INSECURE_TOKEN
    /**
     * For testing purposes where you can not provide an SSL connection:
     * Some browsers (i.e. Safari) don't send a Secure token on non-https
     * connections even for testing with localhost (https requires 'Secure;')
     * You can set an environment variable on .zshrc or .bashrc:
     * export ALLOW_INSECURE_TOKEN=1
     * Then, inside the Makefile, there is a rule to add a preprocessor flag:
     * ifdef ALLOW_INSECURE_TOKEN
     * CFLAGS += -DALLOW_INSECURE_TOKEN
     * endif
     * Depending on this flag, the 'Secure;' flag is sent or not to the client.
     */
    snprintf(session->cookie, COOKIE_SIZE,
        "Set-Cookie: session=%d:%d:%s; "
        "Path=/; HttpOnly; SameSite=Strict; Max-Age=%d\r\n",
        session->user, session->role, session->token, max_age);
#else
    snprintf(session->cookie, COOKIE_SIZE,
        "Set-Cookie: session=%d:%d:%s; "
        "Path=/; Secure; HttpOnly; SameSite=Strict; Max-Age=%d\r\n",
        session->user, session->role, session->token, max_age);
#endif
    return session->token;
}

const char *session_clear(session_t *session)
{
    memset(session->token, 0, TOKEN_MAX_LENGTH);
    snprintf(session->cookie, COOKIE_SIZE, COOKIE_CLEAR);
    return session->token;
}

