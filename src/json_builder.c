/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdlib.h>
#include "clib_math.h"
#include "json_private.h"
#include "json_parser.h"
#include "json_builder.h"

typedef struct
{
    json_t *node;
    unsigned size, height;
    unsigned depth[JSON_MAX_DEPTH];
} json_builder_t;

/**
 * Callback for json_parse.
 * Appends each node in DFS pre-order; containers track their index in
 * builder->depth[] so children can increment the parent's size.
 * height records the deepest event->depth seen — used later to decide
 * whether the fast path applies.
 * node->child is initialised to NULL for containers: the fast path relies
 * on this for empty nested containers (size == 0, child must be NULL).
 */
static int build(const json_event_t *event)
{
    json_builder_t *builder = event->cookie;

    if (event->type & JSON_ITERABLE_END)
    {
        unsigned index = builder->depth[event->depth];

        if (builder->node[index].size > 0)
        {
            builder->node[index].span = builder->size - index;
        }
        return 1;
    }
    if (event->depth > builder->height)
    {
        builder->height = event->depth;
    }

    unsigned index = builder->size;
    unsigned size = next_size(index);
    json_t *node;

    if (size > builder->size)
    {
        node = realloc(builder->node, sizeof(*node) * size);
        if (node == NULL)
        {
            return 0;
        }
        builder->node = node;
    }
    node = &builder->node[index];
    node->key = event->key;
    switch (event->type)
    {
        case JSON_OBJECT:
        case JSON_ARRAY:
            builder->depth[event->depth] = index;
            node->child = NULL;
            break;
        case JSON_STRING:
            node->string = event->string;
            break;
        case JSON_INTEGER:
        case JSON_REAL:
            node->number = event->number;
            break;
        default:
            break;
    }
    node->type = event->type;
    node->size = 0;
    builder->size++;
    if (event->depth > 0)
    {
        builder->node[builder->depth[event->depth - 1]].size++;
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
    json_t *target = malloc(size * sizeof(*target));

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

        if (node->size > 0)
        {
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
    }
    return target;
}

json_t *json_decode(char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    json_builder_t builder = { 0 };

    if (!json_parse(str, build, &builder))
    {
        free(builder.node);
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
     * When true, only one or two child pointers need to be wired.
     */
    unsigned offset = builder.node->size;

    if (builder.node[offset].size + offset + 1 == builder.size)
    {
        if (builder.height > 0)
        {
            builder.node->child = &builder.node[1];
        }
        if (builder.height > 1)
        {
            builder.node[offset].child = &builder.node[offset + 1];
        }
        return builder.node;
    }

    /**
     * General path — nested JSON that requires a true BFS reorder.
     *
     * fixup() allocates a new buffer and rewrites child pointers.
     */
    json_t *node = fixup(builder.node, builder.size);

    free(builder.node);
    return node;
}

