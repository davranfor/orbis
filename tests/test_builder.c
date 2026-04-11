/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "json_private.h"
#include "json_builder.h"

/* NULL and malformed input */
static void test_invalid(void)
{
    TEST(json_decode(NULL) == NULL);

    char empty[] = "";
    TEST(json_decode(empty) == NULL);

    char bad1[] = "not json";
    TEST(json_decode(bad1) == NULL);

    char bad2[] = "{";
    TEST(json_decode(bad2) == NULL);

    char bad3[] = "[1,2,";
    TEST(json_decode(bad3) == NULL);

    char bad4[] = "{\"a\":}";
    TEST(json_decode(bad4) == NULL);
}

/* Scalar values at root level */
static void test_scalars(void)
{
    char s1[] = "42";
    json_t *n = json_decode(s1);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_INTEGER); TEST(n->number == 42); TEST(n->size == 0); free(n); }

    char s2[] = "3.14";
    n = json_decode(s2);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_REAL); TEST(n->size == 0); free(n); }

    char s3[] = "\"hello\"";
    n = json_decode(s3);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_STRING); TEST(strcmp(n->string, "hello") == 0); free(n); }

    char s4[] = "true";
    n = json_decode(s4);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_TRUE); TEST(n->size == 0); free(n); }

    char s5[] = "false";
    n = json_decode(s5);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_FALSE); TEST(n->size == 0); free(n); }

    char s6[] = "null";
    n = json_decode(s6);
    TEST(n != NULL);
    if (n) { TEST(n->type == JSON_NULL); TEST(n->size == 0); free(n); }
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
 * Flat containers — fast path (height <= 1).
 * DFS pre-order is already BFS: only root->child is wired.
 */
static void test_flat(void)
{
    char s1[] = "[1, 2, 3]";
    json_t *root = json_decode(s1);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->type == JSON_ARRAY);
        TEST(root->size == 3);
        TEST(root->child != NULL);
        TEST(root->child[0].type == JSON_INTEGER);
        TEST(root->child[0].number == 1);
        TEST(root->child[1].number == 2);
        TEST(root->child[2].number == 3);
        /* Children contiguous immediately after root */
        TEST(root->child == root + 1);
        free(root);
    }

    char s2[] = "{\"a\": 1, \"b\": 2, \"c\": 3}";
    root = json_decode(s2);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->type == JSON_OBJECT);
        TEST(root->size == 3);
        TEST(root->child != NULL);
        TEST(strcmp(root->child[0].key, "a") == 0); TEST(root->child[0].number == 1);
        TEST(strcmp(root->child[1].key, "b") == 0); TEST(root->child[1].number == 2);
        TEST(strcmp(root->child[2].key, "c") == 0); TEST(root->child[2].number == 3);
        free(root);
    }

    /* Mixed types in flat array */
    char s3[] = "[1, \"x\", true, false, null]";
    root = json_decode(s3);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->size == 5);
        TEST(root->child[0].type == JSON_INTEGER);
        TEST(root->child[1].type == JSON_STRING);
        TEST(root->child[2].type == JSON_TRUE);
        TEST(root->child[3].type == JSON_FALSE);
        TEST(root->child[4].type == JSON_NULL);
        free(root);
    }
}

/*
 * Nested containers — general path (height > 1).
 * Verifies BFS layout: siblings contiguous, child pointers correct.
 */
static void test_nested(void)
{
    /* [[1,2],[3,4]] — two level-1 arrays must be adjacent (BFS) */
    char s1[] = "[[1, 2], [3, 4]]";
    json_t *root = json_decode(s1);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->type == JSON_ARRAY);
        TEST(root->size == 2);

        json_t *a = &root->child[0];
        json_t *b = &root->child[1];

        TEST(a->type == JSON_ARRAY); TEST(a->size == 2);
        TEST(b->type == JSON_ARRAY); TEST(b->size == 2);

        /* BFS: level-1 siblings are contiguous */
        TEST(b == a + 1);

        TEST(a->child[0].number == 1);
        TEST(a->child[1].number == 2);
        TEST(b->child[0].number == 3);
        TEST(b->child[1].number == 4);

        free(root);
    }

    /* {"x": {"y": 42}} */
    char s2[] = "{\"x\": {\"y\": 42}}";
    root = json_decode(s2);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->type == JSON_OBJECT);
        TEST(root->size == 1);

        json_t *x = &root->child[0];
        TEST(x->type == JSON_OBJECT);
        TEST(strcmp(x->key, "x") == 0);
        TEST(x->size == 1);

        json_t *y = &x->child[0];
        TEST(y->type == JSON_INTEGER);
        TEST(strcmp(y->key, "y") == 0);
        TEST(y->number == 42);

        free(root);
    }

    /* Deep nesting [[[1]]] */
    char s3[] = "[[[1]]]";
    root = json_decode(s3);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->type == JSON_ARRAY); TEST(root->size == 1);
        json_t *l1 = &root->child[0];
        TEST(l1->type == JSON_ARRAY); TEST(l1->size == 1);
        json_t *l2 = &l1->child[0];
        TEST(l2->type == JSON_ARRAY); TEST(l2->size == 1);
        TEST(l2->child[0].number == 1);
        free(root);
    }
}

/* Empty containers nested inside general-path tree */
static void test_empty_nested(void)
{
    /* [[], 1, [2]] — outer array has one empty child and one non-empty */
    char s1[] = "[[], 1, [2]]";
    json_t *root = json_decode(s1);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->size == 3);
        TEST(root->child[0].type == JSON_ARRAY);
        TEST(root->child[0].size == 0);
        TEST(root->child[0].child == NULL);
        TEST(root->child[1].type == JSON_INTEGER);
        TEST(root->child[1].number == 1);
        TEST(root->child[2].type == JSON_ARRAY);
        TEST(root->child[2].size == 1);
        TEST(root->child[2].child[0].number == 2);
        free(root);
    }

    /* {"a": {}, "b": {"c": 1}} — one empty object sibling */
    char s2[] = "{\"a\": {}, \"b\": {\"c\": 1}}";
    root = json_decode(s2);
    TEST(root != NULL);
    if (root)
    {
        TEST(root->size == 2);
        json_t *a = &root->child[0];
        json_t *b = &root->child[1];
        TEST(strcmp(a->key, "a") == 0);
        TEST(a->type == JSON_OBJECT); TEST(a->size == 0); TEST(a->child == NULL);
        TEST(strcmp(b->key, "b") == 0);
        TEST(b->type == JSON_OBJECT); TEST(b->size == 1);
        TEST(strcmp(b->child[0].key, "c") == 0);
        TEST(b->child[0].number == 1);
        free(root);
    }
}

/* Single free() releases the whole tree */
static void test_single_free(void)
{
    char s[] = "{\"a\": {\"b\": [1, 2, 3]}, \"c\": true}";
    json_t *root = json_decode(s);
    TEST(root != NULL);
    if (root)
    {
        /* Navigate to verify tree is intact */
        TEST(root->child[0].child[0].child[2].number == 3);
        free(root); /* must not leak or crash */
    }
}

int main(void)
{
    test_invalid();
    test_scalars();
    test_empty_containers();
    test_flat();
    test_nested();
    test_empty_nested();
    test_single_free();

    TEST_SUMMARY();
}
