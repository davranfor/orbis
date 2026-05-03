#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <orbis/clib_stream.h>
#include <orbis/json_writer.h>
#include <orbis/json_buffer.h>

static int decode(char *text)
{
    json_t *node = json_decode(text);

    if (node == NULL)
    {
        perror("json_decode");
        return 0;
    }

    int rc = json_print(node);

    if (rc == 0)
    {
        perror("json_print");
    }
    free(node);
    return rc;
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

