#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <orbis/clib_stream.h>
#include <orbis/json_struct.h>

static int compile(char *text)
{
    void *code = json_compile(text);

    if (code == NULL)
    {
        perror("json_compile");
        return 0;
    }
    json_validate(NULL, code);
    free(code);
    return 1;
}

int main(int argc, char *argv[])
{
    setlocale(LC_NUMERIC, "C");

    char *text = file_read(argc > 1 ? argv[1] : "test.lisp");

    if (text == NULL)
    {
        perror("file_read");
        exit(EXIT_FAILURE);
    }

    int rc = compile(text) ? EXIT_SUCCESS : EXIT_FAILURE;

    free(text);
    return rc;
}

