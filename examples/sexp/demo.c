#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <orbis/clib_stream.h>
#include <orbis/sexp_parser.h>

int print(const sexp_event_t *event)
{
    for (unsigned depth = 0; depth < event->depth; depth++)
    {
        printf("  ");
    }
    switch (event->type)
    {
        case SEXP_SYMBOL:
            printf("(%s\n", event->string);
            break;
        case SEXP_SYMBOL_END:
            printf(")\n");
            break;
        case SEXP_STRING:
            printf("\"%s\"\n", event->string);
            break;
        case SEXP_INTEGER:
            printf("%.0f\n", event->number);
            break;
        case SEXP_REAL:
            printf("%g\n", event->number);
            break;
        case SEXP_TRUE:
            printf("true\n");
            break;
        case SEXP_FALSE:
            printf("false\n");
            break;
        case SEXP_NULL:
            printf("null\n");
            break;
    }
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

    int rc = sexp_parse(text, print, NULL);

    if (!rc)
    {
        fprintf(stderr, "Invalid S-expression\n");
    }
    free(text);
    return rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

