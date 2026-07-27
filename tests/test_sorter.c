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
#include "json_sorter.h"

/* Ascending integer comparator */
static int cmp_int_asc(const void *a, const void *b)
{
    const json_t *na = a;
    const json_t *nb = b;

    if (na->number < nb->number) return -1;
    if (na->number > nb->number) return  1;
    return 0;
}

/* Descending integer comparator */
static int cmp_int_desc(const void *a, const void *b)
{
    return -cmp_int_asc(a, b);
}

/* Comparator for json_search: key is a plain double */
static int cmp_key_double(const void *key, const void *elem)
{
    double k = *(const double *)key;
    const json_t *n = elem;

    if (k < n->number) return -1;
    if (k > n->number) return  1;
    return 0;
}

/* String comparator (ascending, by node->string) */
static int cmp_str_asc(const void *a, const void *b)
{
    const json_t *na = a;
    const json_t *nb = b;
    return strcmp(na->string, nb->string);
}

/* NULL safety and size <= 1 short-circuits */
static void test_null_safety(void)
{
    json_sort(NULL, cmp_int_asc);       /* must not crash */
    json_reverse(NULL);                 /* must not crash */

    TEST(json_search(NULL, NULL, cmp_int_asc) == NULL);

    char s[] = "[1]";
    json_t *one = json_decode(s);
    TEST(one != NULL);
    if (one)
    {
        json_sort(one, cmp_int_asc);    /* size <= 1, no-op */
        json_reverse(one);              /* size <= 1, no-op */
        TEST(one->child[0].number == 1);
        free(one);
    }

    char s2[] = "[]";
    json_t *empty = json_decode(s2);
    TEST(empty != NULL);
    if (empty)
    {
        double key = 0;
        TEST(json_search(empty, &key, cmp_key_double) == NULL);
        free(empty);
    }
}

/* json_sort over integers */
static void test_sort_int(void)
{
    char s[] = "[3, 1, 4, 1, 5, 9, 2, 6]";
    json_t *arr = json_decode(s);
    TEST(arr != NULL);
    if (!arr) return;

    json_sort(arr, cmp_int_asc);

    TEST(arr->child[0].number == 1);
    TEST(arr->child[1].number == 1);
    TEST(arr->child[2].number == 2);
    TEST(arr->child[3].number == 3);
    TEST(arr->child[4].number == 4);
    TEST(arr->child[5].number == 5);
    TEST(arr->child[6].number == 6);
    TEST(arr->child[7].number == 9);

    /* Re-sort descending */
    json_sort(arr, cmp_int_desc);
    TEST(arr->child[0].number == 9);
    TEST(arr->child[7].number == 1);

    free(arr);
}

/* json_sort over strings */
static void test_sort_str(void)
{
    char s[] = "[\"banana\", \"apple\", \"cherry\"]";
    json_t *arr = json_decode(s);
    TEST(arr != NULL);
    if (!arr) return;

    json_sort(arr, cmp_str_asc);

    TEST(strcmp(arr->child[0].string, "apple")  == 0);
    TEST(strcmp(arr->child[1].string, "banana") == 0);
    TEST(strcmp(arr->child[2].string, "cherry") == 0);

    free(arr);
}

/* json_search on a sorted array */
static void test_search(void)
{
    char s[] = "[10, 20, 30, 40, 50]";
    json_t *arr = json_decode(s);
    TEST(arr != NULL);
    if (!arr) return;

    /* Already sorted ascending */
    double key = 30;
    json_t *hit = json_search(arr, &key, cmp_key_double);
    TEST(hit != NULL);
    if (hit) { TEST(hit->number == 30); }

    /* Boundary hits */
    key = 10;
    TEST(json_search(arr, &key, cmp_key_double) == &arr->child[0]);
    key = 50;
    TEST(json_search(arr, &key, cmp_key_double) == &arr->child[4]);

    /* Miss */
    key = 25;
    TEST(json_search(arr, &key, cmp_key_double) == NULL);

    free(arr);
}

/* json_reverse */
static void test_reverse(void)
{
    char s[] = "[1, 2, 3, 4, 5]";
    json_t *arr = json_decode(s);
    TEST(arr != NULL);
    if (!arr) return;

    json_reverse(arr);

    TEST(arr->child[0].number == 5);
    TEST(arr->child[1].number == 4);
    TEST(arr->child[2].number == 3);
    TEST(arr->child[3].number == 2);
    TEST(arr->child[4].number == 1);

    /* Reversing twice → original */
    json_reverse(arr);
    TEST(arr->child[0].number == 1);
    TEST(arr->child[4].number == 5);

    free(arr);

    /* Even-length: verify the swap boundary */
    char s2[] = "[1, 2, 3, 4]";
    arr = json_decode(s2);
    TEST(arr != NULL);
    if (arr)
    {
        json_reverse(arr);
        TEST(arr->child[0].number == 4);
        TEST(arr->child[3].number == 1);
        free(arr);
    }
}

int main(void)
{
    test_null_safety();
    test_sort_int();
    test_sort_str();
    test_search();
    test_reverse();

    TEST_SUMMARY();
}
