/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <orbis/clib_stream.h>
#include <orbis/json_reader.h>
#include <orbis/json_decoder.h>

int main(int argc, char *argv[])
{
    setlocale(LC_NUMERIC, "C");

    char *text = file_read(argc > 1 ? argv[1] : "test.json");

    if (text == NULL)
    {
        perror("file_read");
        exit(EXIT_FAILURE);
    }

    json_t *root = json_decode(text);

    if (root == NULL)
    {
        fprintf(stderr, "Error decoding file\n");
    }
    for (unsigned i = 0, n = json_is_array(root) ? json_size(root) : 0; i < n; i++)
    {
        const json_t *node = json_at(root, i);

        printf("Testing '%s' -> '%s' = %s\n",
            json_name(json_at(node, 1)),
            json_text(json_at(node, 1)),
            json_match(json_at(node, 1), json_string(json_at(node, 0))) ? "true" : "false"
        );
    }
    free(root);
    free(text);
    return 0;
}

