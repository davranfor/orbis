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
#include "json_pointer.h"

/* NULL path and empty path */
static void test_null_and_root(void)
{
    char s[] = "{\"a\": 1}";
    json_t *root = json_decode(s);
    TEST(root != NULL);
    if (!root) return;

    TEST(json_pointer(root, NULL) == NULL);
    TEST(json_pointer(root, "")   == root);  /* "" → root itself */
    TEST(json_pointer(root, "a")  == NULL);  /* no leading '/' → invalid */

    free(root);
}

/* Object key lookup */
static void test_object(void)
{
    char s[] = "{\"name\": \"Alice\", \"age\": 30, \"active\": true}";
    json_t *root = json_decode(s);
    TEST(root != NULL);
    if (!root) return;

    json_t *name = json_pointer(root, "/name");
    TEST(name != NULL);
    if (name) { TEST(name->type == JSON_STRING); TEST(strcmp(name->string, "Alice") == 0); }

    json_t *age = json_pointer(root, "/age");
    TEST(age != NULL);
    if (age) { TEST(age->type == JSON_INTEGER); TEST(age->number == 30); }

    json_t *active = json_pointer(root, "/active");
    TEST(active != NULL);
    if (active) { TEST(active->type == JSON_TRUE); }

    TEST(json_pointer(root, "/missing") == NULL);

    free(root);
}

/* Array index lookup */
static void test_array(void)
{
    char s[] = "[10, 20, 30]";
    json_t *root = json_decode(s);
    TEST(root != NULL);
    if (!root) return;

    json_t *n0 = json_pointer(root, "/0");
    TEST(n0 != NULL);
    if (n0) { TEST(n0->type == JSON_INTEGER); TEST(n0->number == 10); }

    json_t *n2 = json_pointer(root, "/2");
    TEST(n2 != NULL);
    if (n2) { TEST(n2->number == 30); }

    /* Out of bounds */
    TEST(json_pointer(root, "/3")  == NULL);
    TEST(json_pointer(root, "/99") == NULL);

    /* Non-numeric index on array */
    TEST(json_pointer(root, "/key") == NULL);

    free(root);
}

/* Nested navigation */
static void test_nested(void)
{
    char s[] = "{\"a\": {\"b\": {\"c\": 42}}}";
    json_t *root = json_decode(s);
    TEST(root != NULL);
    if (!root) return;

    json_t *a = json_pointer(root, "/a");
    TEST(a != NULL);
    if (a) { TEST(a->type == JSON_OBJECT); }

    json_t *b = json_pointer(root, "/a/b");
    TEST(b != NULL);
    if (b) { TEST(b->type == JSON_OBJECT); }

    json_t *c = json_pointer(root, "/a/b/c");
    TEST(c != NULL);
    if (c) { TEST(c->type == JSON_INTEGER); TEST(c->number == 42); }

    TEST(json_pointer(root, "/a/b/x") == NULL);
    TEST(json_pointer(root, "/a/x/c") == NULL);

    free(root);
}

/* Mixed: object containing array */
static void test_mixed(void)
{
    char s[] = "{\"scores\": [10, 20, 30]}";
    json_t *root = json_decode(s);
    TEST(root != NULL);
    if (!root) return;

    json_t *scores = json_pointer(root, "/scores");
    TEST(scores != NULL);
    if (scores) { TEST(scores->type == JSON_ARRAY); TEST(scores->size == 3); }

    json_t *first = json_pointer(root, "/scores/0");
    TEST(first != NULL);
    if (first) { TEST(first->number == 10); }

    json_t *last = json_pointer(root, "/scores/2");
    TEST(last != NULL);
    if (last) { TEST(last->number == 30); }

    TEST(json_pointer(root, "/scores/3") == NULL);

    free(root);
}

/* RFC 6901 escape sequences: ~0 → ~, ~1 → / */
static void test_escapes(void)
{
    /* Key containing '~' → escaped as '~0' in path */
    char s1[] = "{\"a~b\": 1}";
    json_t *root = json_decode(s1);
    TEST(root != NULL);
    if (root)
    {
        TEST(json_pointer(root, "/a~0b") != NULL);
        TEST(json_pointer(root, "/a~b")  == NULL); /* unescaped ~ is wrong */
        free(root);
    }

    /* Key containing '/' → escaped as '~1' in path */
    char s2[] = "{\"a/b\": 2}";
    root = json_decode(s2);
    TEST(root != NULL);
    if (root)
    {
        TEST(json_pointer(root, "/a~1b") != NULL);
        TEST(json_pointer(root, "/a/b")  == NULL); /* '/' is a separator, not part of key */
        free(root);
    }
}

int main(void)
{
    test_null_and_root();
    test_object();
    test_array();
    test_nested();
    test_mixed();
    test_escapes();

    TEST_SUMMARY();
}
