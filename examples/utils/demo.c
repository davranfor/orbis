#include <stdio.h>
#include <stdlib.h>
#include <orbis/json_builder.h>
#include <orbis/json_buffer.h>
#include <orbis/json_reader.h>
#include <orbis/json_utils.h>

static int sort(const void *pa, const void *pb)
{
    int a = json_int((const json_t *)pa);
    int b = json_int((const json_t *)pb);

    return (a > b) - (a < b);
}

static int search(const void *key, const void *node)
{
    int a = *(const int *)key;
    int b = json_int((const json_t *)node);

    return (a > b) - (a < b);
}

int main(void)
{
    char text[] = "[2, 9, 3, 5, 1, 8, 4, 0, 6, 7]";
    json_t *array = json_decode(text);

    if (array == NULL)
    {
        perror("json_decode");
        return 0;
    }
    puts("Unordered");
    json_print(array);
    puts("Ordered");
    json_sort(array, sort);
    json_print(array);
    puts("Search number 5 in array");
    json_print(json_search(array, &(int []){5}, search));
    puts("Reversed");
    json_reverse(array);
    json_print(array);
    free(array);
}

