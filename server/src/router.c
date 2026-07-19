/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/clib_stream.h>
#include <orbis/json_validator.h>
#include "router.h" 

struct
{
    endpoint_t *endpoint;
    size_t size, room;
} router;

enum
{
    NONE = 0x0,
    PATH = 0x1,
    EVAL = 0x2,
    STMT = 0x4,
    DONE = 0x8
};

static char *FAILURE = "";
static char *buffer;

static endpoint_t *endpoint_resize(void)
{
    if (router.size == router.room)
    {
        size_t room = router.room ? router.room * 2 : 1;
        endpoint_t *endpoint = realloc(router.endpoint, sizeof(*endpoint) * room);

        if (endpoint == NULL)
        {
            return NULL;
        }
        router.endpoint = endpoint;
        router.room = room;
    }
    memset(&router.endpoint[router.size], 0, sizeof(endpoint_t));
    return &router.endpoint[router.size++];
}

int router_method(const char *str)
{
    static const char *methods[] =
    {
        "GET /api/", "POST /api/", "PUT /api/", "PATCH /api/", "DELETE /api/"
    };

    for (int i = 0; i < (int)(sizeof methods / sizeof methods[0]); i++)
    {
        if (!strncmp(str, methods[i], strlen(methods[i])))
        {
            return i + 1;
        }
    }
    return 0;
}

static unsigned get_section(const char *str)
{
    static const char *sections[] = { "-- @path", "-- @eval", "-- @stmt" };

    for (size_t i = 0; i < sizeof sections / sizeof sections[0]; i++)
    {
        size_t length = strlen(sections[i]);

        if ((strncmp(str, sections[i], length) == 0) &&
            (strcspn(str + length, "\n") == strspn(str + length, " \r\t")))
        {
            return 1u << i;
        }
    }
    return *str ? NONE : DONE;
}

static int set_section(unsigned sections, unsigned section, char *str)
{
    endpoint_t *endpoint;

    if (!(sections & (sections - 1)))
    {
        endpoint = endpoint_resize();
        if (endpoint == NULL)
        {
            return 0;
        }
    }
    else
    {
        endpoint = &router.endpoint[router.size - 1];
    }
    switch (section)
    {
        case PATH:
        {
            printf("- %s\n", str);
            if (router_method(str) == 0)
            {
                fprintf(stderr, "METHOD /api/... was expected\n");
                return 0;
            }
            endpoint->path = str;
            break;
        }
        case EVAL:
        {
            endpoint->code = json_compile(str);
            if (endpoint->code == NULL)
            {
                fprintf(stderr, "Can't compile the 'eval' part\n");
                return 0;
            }
            break;
        }
        case STMT:
        {
            if (strncmp(str, "none", 4))
            {
                endpoint->stmt = str;
            }
            break;
        }
    }
    return 1;
}

static char *scan(char *str)
{
    char *start = NULL, *end = NULL;
    unsigned sections = NONE;

    for (unsigned section = NONE, blank = 1; ; str++)
    {
        switch (*str)
        {
            case ' ' :
            case '\t':
            case '\r':
                break;
            case '\n':
                blank = 1;
                break;
            case '\0':
                if ((sections == NONE) && (start == NULL))
                {
                    return NULL;
                }
                goto scanner;
            case '-':
                if (blank && !strncmp(str, "-- @", 4))
                {
                    scanner:
                    if (section != NONE)
                    {
                        if (start == NULL)
                        {
                            fprintf(stderr, "Empty section\n");
                            return FAILURE;
                        }
                        end[1] = '\0';
                        if (!set_section(sections, section, start))
                        {
                            return FAILURE;
                        }
                    }
                    section = get_section(str);
                    if (section == NONE)
                    {
                        fprintf(stderr, "Unknown section\n");
                        return FAILURE;
                    }
                    if ((section == DONE) || (sections & section))
                    {
                        goto done;
                    }
                    sections |= section;
                    str += strcspn(str, "\n");
                    start = NULL;
                    break;
                }
                __attribute__((fallthrough));
            default:
                if (start == NULL)
                {
                    start = str;
                }
                end = str;
                blank = 0;
                break;
        }
    }
done:
    if (sections != (PATH | EVAL | STMT))
    {
        fprintf(stderr, "@path, @eval and @stmt sections are required\n");
        return FAILURE;
    }
    return *str ? str : NULL;
}

static int sort(const void *pa, const void *pb)
{
    const endpoint_t *a = (const endpoint_t *)pa;
    const endpoint_t *b = (const endpoint_t *)pb;

    return strcmp(a->path, b->path);
}

static int search(const void *pa, const void *pb)
{
    const endpoint_t *a = (const endpoint_t *)pa;
    const endpoint_t *b = (const endpoint_t *)pb;

    int rc = strcmp(a->path, b->path);

    if (rc == 0)
    {
        rc = (a->index > b->index) - (a->index < b->index);
    }
    return rc;
}

static void enumerate(void)
{
    endpoint_t *endpoint = router.endpoint;

    for (size_t i = 1; i < router.size; i++)
    {
        if (!strcmp(endpoint[i].path, endpoint[i - 1].path))
        {
            endpoint[i].index = endpoint[i - 1].index + 1;
        }
    }
}

static int parse(char *str)
{
    while ((str = scan(str)))
    {
        if (str == FAILURE)
        {
            return 0;
        }
    }
    if (router.size == 0)
    {
        fprintf(stderr, "Must contain at least one section\n");
        return 0;
    }
    qsort(router.endpoint, router.size, sizeof(endpoint_t), sort);
    enumerate();
    return 1;
}

static void load(void)
{
    const char *path = "api/endpoints.sql";

    printf("Loading '%s'\n", path);
    if (!(buffer = file_read(path)))
    {
        perror("file_read");
        exit(EXIT_FAILURE);
    }
    if (!parse(buffer))
    {
        exit(EXIT_FAILURE);
    }
}

static void unload(void)
{
    for (size_t i = 0; i < router.size; i++)
    {
        free(router.endpoint[i].code);
    }
    free(router.endpoint);
    router.endpoint = NULL;
    router.size = 0;
    router.room = 0;
    free(buffer);
}

void router_load(void)
{
    atexit(unload);
    load();
}

void router_reload(void)
{
    unload();
    load();
}

const endpoint_t *router_search(const char *path, int index)
{
    const endpoint_t endpoint = { .path = path, .index = index };

    return bsearch(&endpoint, router.endpoint, router.size, sizeof(endpoint_t), search);
}

