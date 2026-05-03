#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <orbis/json_parser.h>

int print(const json_event_t *event)
{
    for (unsigned depth = 0; depth < event->depth; depth++)
    {
        printf("  ");
    }
    if (event->key != NULL)
    {
        printf("%s: ", event->key);
    }
    switch (event->type)
    {
        case JSON_OBJECT:
            printf("{\n");
            break;
        case JSON_OBJECT_END:
            printf("}\n");
            break;
        case JSON_ARRAY:
            printf("[\n");
            break;
        case JSON_ARRAY_END:
            printf("]\n");
            break;
        case JSON_STRING:
            printf("%s\n", event->string);
            break;
        case JSON_INTEGER:
            printf("%.0f\n", event->number);
            break;
        case JSON_REAL:
            printf("%g\n", event->number);
            break;
        case JSON_TRUE:
            printf("true\n");
            break;
        case JSON_FALSE:
            printf("false\n");
            break;
        case JSON_NULL:
            printf("null\n");
            break;
    }
    return 1;
}

int main(void)
{
    setlocale(LC_NUMERIC, "C");

    char text[] = "[\"start\", {\"array\": [1, 2, 3]}, 3.14, true, false, null, \"end\"]";

    if (!json_parse(text, print, NULL))
    {
        fprintf(stderr, "Invalid JSON\n");
        exit(EXIT_FAILURE);
    }
}

