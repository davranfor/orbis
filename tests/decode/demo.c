#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <orbis/clib_stream.h>
#include <orbis/json_builder.h>
#include <orbis/json_buffer.h>

static int decode(char *text)
{
    json_t *node = json_decode(text);

    if (node == NULL)
    {
        perror("json_decode");
        return 0;
    }
    json_print(node);
    free(node);
    return 1;
}

int main(int argc, char *argv[])
{
    setlocale(LC_NUMERIC, "C");

    char *text = file_read(argc > 1 ? argv[1] : "test.json");

    if (text == NULL)
    {
        perror("file_read");
        exit(EXIT_FAILURE);
    }

    int rc = decode(text) ? EXIT_SUCCESS : EXIT_FAILURE;

    free(text);
    return rc;
}

