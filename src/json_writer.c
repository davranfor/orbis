/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdlib.h>
#include "json_private.h"
#include "json_parser.h"
#include "json_writer.h"

typedef struct
{
    json_t *node;
    unsigned size, room;
    unsigned depth[JSON_MAX_DEPTH];
} pool_t;

/**
 * Callback for json_parse.
 * Appends each node in DFS pre-order; containers track their index in
 * pool->depth[] so children can increment the parent's size.
 * node->child is initialised to NULL for containers: the fast path relies
 * on this for empty nested containers (size == 0, child must be NULL).
 */
static int decode(const json_event_t *event)
{
    pool_t *pool = event->data;

    if (event->type & JSON_ITERABLE_END)
    {
        unsigned index = pool->depth[event->depth];

        if (pool->node[index].size > 0)
        {
            pool->node[index].span = pool->size - index;
        }
        return 1;
    }

    json_t *node;

    if (pool->size == pool->room)
    {
        unsigned room = pool->room ? pool->room * 2 : 1;

        node = realloc(pool->node, sizeof(*node) * room);
        if (node == NULL)
        {
            return 0;
        }
        pool->node = node;
        pool->room = room;
    }
    node = &pool->node[pool->size];
    node->key = event->key;
    switch (event->type)
    {
        case JSON_OBJECT:
        case JSON_ARRAY:
            pool->depth[event->depth] = pool->size;
            node->child = NULL;
            break;
        case JSON_STRING:
            node->string = event->string;
            break;
        case JSON_INTEGER:
        case JSON_REAL:
            node->number = event->number;
            break;
    }
    node->type = event->type;
    node->size = 0;
    pool->size++;
    if (event->depth > 0)
    {
        pool->node[pool->depth[event->depth - 1]].size++;
    }
    return 1;
}

/**
 * Full DFS -> BFS reorder using a temporary buffer.
 *
 * The BFS buffer doubles as an implicit BFS queue: reader dequeues,
 * writer enqueues.  When a container is dequeued, its children are
 * located in the DFS array using the DFS index stored in node->index,
 * then appended to the BFS buffer with their real positions resolved.
 * node->span holds the precomputed subtree size of each container,
 * set during build() at the END event — this replaces the recursive
 * population() traversal with an O(1) lookup per child.
 *
 * Returns the BFS buffer (caller frees the original DFS buffer).
 * Returns NULL on allocation failure (DFS buffer not freed — caller
 * must free it).
 */
static json_t *fixup(const json_t *source, unsigned size)
{
    json_t *target = malloc(sizeof(*target) * size);

    if (target == NULL)
    {
        return NULL;
    }
    target[0] = source[0];

    unsigned reader = 0;
    unsigned writer = 1;

    while (reader < writer)
    {
        json_t *node = &target[reader++];

        if (node->size == 0)
        {
            continue;
        }

        unsigned cursor = node->index + 1;

        node->child = &target[writer];
        for (unsigned i = 0; i < node->size; i++)
        {
            target[writer] = source[cursor];
            if (target[writer].size > 0)
            {
                target[writer].index = cursor;
                cursor += target[writer].span;
            }
            else
            {
                cursor += 1;
            }
            writer++;
        }
    }
    return target;
}

json_t *json_decode(char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    pool_t pool = { 0 };

    if (!json_parse(str, decode, &pool))
    {
        free(pool.node);
        return NULL;
    }

    /**
     * Fast path — DFS and BFS layouts are identical when:
     * - all of root's children except possibly the last are leaves, AND
     * - the last child (if a container) has only leaf children.
     *
     * offset = root->size. In DFS pre-order, if the first offset-1 children
     * of root are leaves they occupy positions 1..offset-1, so position offset
     * is the last direct child. The condition verifies that nothing follows
     * that child's own leaf children: offset + node[offset].size + 1 == total.
     * When true, at most two child pointers need to be wired: root's and,
     * if node[offset] is a non-empty container, its own.
     */
    unsigned offset = pool.node->size;

    if (pool.node[offset].size + offset + 1 == pool.size)
    {
        if (pool.node->size > 0)
        {
            pool.node->child = &pool.node[1];
        }
        if (pool.node[offset].size > 0)
        {
            pool.node[offset].child = &pool.node[offset + 1];
        }
        return pool.node;
    }

    /**
     * General path — nested JSON that requires a true BFS reorder.
     *
     * fixup() allocates a new buffer and rewrites child pointers.
     */
    json_t *node = fixup(pool.node, pool.size);

    free(pool.node);
    return node;
}

