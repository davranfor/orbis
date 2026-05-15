/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <orbis/clib_stream.h>
#include <orbis/json_writer.h>
#include <orbis/json_validator.h>

int main(int argc, char *argv[])
{
    int rc = EXIT_FAILURE;

    setlocale(LC_NUMERIC, "C");

    const char *path[] =
    {
        argc > 1 ? argv[1] : "test.json",
        argc > 2 ? argv[2] : "test.lisp"
    };

    char *file[] = { NULL, NULL };
    json_t *node = NULL;
    void *code = NULL;

    if (!(file[0] = file_read(path[0])))
    {
        fprintf(stderr, "%s\n", path[0]);
        perror("file_read");
        goto stop;
    }
    if (!(file[1] = file_read(path[1])))
    {
        fprintf(stderr, "%s\n", path[1]);
        perror("file_read");
        goto stop;
    }
    if (!(node = json_decode(file[0])))
    {
        perror("json_decode");
        goto stop;
    }
    if (!(code = json_compile(file[1])))
    {
        fprintf(stderr, "'%s' doesn't compile\n", path[1]);
        goto stop;
    }
    if (!json_validate(node, code, NULL, NULL))
    {
        fprintf(stderr, "'%s' doesn't validate against '%s'\n", path[0], path[1]);
        goto stop;
    }
    rc = EXIT_SUCCESS;
stop:
    free(file[0]);
    free(file[1]);
    free(node);
    free(code);
    return rc;
}

