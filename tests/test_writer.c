/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "json_private.h"
#include "json_writer.h"

/* NULL and malformed input */
static void test_invalid(void)
{
    TEST(json_decode(NULL) == NULL);

    char empty[] = "";
    TEST(json_decode(empty) == NULL);

    char bad1[] = "{";
    TEST(json_decode(bad1) == NULL);

    char bad2[] = "[1,2,";
    TEST(json_decode(bad2) == NULL);

    char bad3[] = "{\"a\":}";
    TEST(json_decode(bad3) == NULL);
}

/* Scalar values at root level */
static void test_scalars(void)
{
    char s1[] = "42";
    json_t *n = json_decode(s1);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_INTEGER); TEST(n->number == 42); TEST(n->size == 0); free(n); }

    char s2[] = "\"hello\"";
    n = json_decode(s2);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_STRING); TEST(strcmp(n->string, "hello") == 0); free(n); }

    char s3[] = "true";
    n = json_decode(s3);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_TRUE); free(n); }

    char s4[] = "null";
    n = json_decode(s4);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_NULL); free(n); }
}

/* Empty containers */
static void test_empty_containers(void)
{
    char s1[] = "{}";
    json_t *n = json_decode(s1);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_OBJECT); TEST(n->size == 0); TEST(n->child == NULL); free(n); }

    char s2[] = "[]";
    n = json_decode(s2);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_ARRAY); TEST(n->size == 0); TEST(n->child == NULL); free(n); }
}

/*
 * Flat containers — fast path: DFS pre-order is already BFS,
 * only root->child is wired and children are contiguous.
 */
static void test_flat(void)
{
    char s1[] = "[1, 2, 3]";
    json_t *root = json_decode(s1);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->size == 3);
        TEST(root->child == root + 1);   /* contiguous fast-path layout */
        TEST(root->child[0].number == 1);
        TEST(root->child[2].number == 3);
        free(root);
    }

    char s2[] = "{\"a\": 1, \"b\": 2}";
    root = json_decode(s2);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->size == 2);
        TEST(strcmp(root->child[0].key, "a") == 0); TEST(root->child[0].number == 1);
        TEST(strcmp(root->child[1].key, "b") == 0); TEST(root->child[1].number == 2);
        free(root);
    }
}

/* Nested containers — general path: BFS layout, siblings contiguous */
static void test_nested(void)
{
    char s1[] = "[[1, 2], [3, 4]]";
    json_t *root = json_decode(s1);
    TEST(root != NULL);
    if (root)
    {
        json_t *a = &root->child[0];
        json_t *b = &root->child[1];

        TEST(b == a + 1);                /* BFS: level-1 siblings contiguous */
        TEST(a->child[1].number == 2);
        TEST(b->child[0].number == 3);
        free(root);
    }

    char s2[] = "{\"x\": {\"y\": 42}}";
    root = json_decode(s2);
    TEST(root != NULL);
    if (root)
    {
        json_t *x = &root->child[0];
        TEST(strcmp(x->key, "x") == 0);
        TEST(x->size == 1);
        TEST(strcmp(x->child[0].key, "y") == 0);
        TEST(x->child[0].number == 42);
        free(root);
    }
}

/* Single free() releases the whole tree (one malloc) */
static void test_single_free(void)
{
    char s[] = "{\"a\": {\"b\": [1, 2, 3]}, \"c\": true}";
    json_t *root = json_decode(s);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->child[0].child[0].child[2].number == 3);
        free(root);   /* must release everything */
    }
}

int main(void)
{
    test_invalid();
    test_scalars();
    test_empty_containers();
    test_flat();
    test_nested();
    test_single_free();

    TEST_SUMMARY();
}
