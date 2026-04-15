/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdlib.h>
#include "json_private.h"
#include "json_utils.h"

/* Search a node into a json iterable (the iterable must be already sorted) */
json_t *json_search(const json_t *node, const void *key,
    json_sort_callback callback)
{
    if ((node == NULL) || (node->size == 0) || (key == NULL))
    {
        return NULL;
    }

    return bsearch(key, node->child, node->size, sizeof *node->child, callback);
}

/* Sorts a json iterable using qsort */
void json_sort(json_t *node, json_sort_callback callback)
{
    if ((node == NULL) || (node->size <= 1))
    {
        return;
    }
    qsort(node->child, node->size, sizeof *node->child, callback);
}

/* Reverses a json iterable */
void json_reverse(json_t *node)
{
    if ((node == NULL) || (node->size <= 1))
    {
        return;
    }

    unsigned lower = 0, upper = node->size - 1;

    while (lower < upper)
    {
        json_t temp = node->child[lower];

        node->child[lower++] = node->child[upper];
        node->child[upper--] = temp;
    }
}

