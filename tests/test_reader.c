/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test.h"
#include "json_builder.h"
#include "json_reader.h"

/*
 * Global fixtures decoded once in main(), freed at exit.
 * Strings must be writable — the parser terminates substrings in-place.
 *
 * flat:    {"a":1,"b":"hello","c":true,"d":false,"e":null,"f":3.14}
 * nested:  {"x":{"y":[1,2,3]},"z":42}
 * arr:     [10,20,30]
 * j_int:   99
 * j_real:  1.5
 * j_str:   "world"
 * j_true:  true
 * j_false: false
 * j_null:  null
 * j_eobj:  {}
 * j_earr:  []
 */
static char s_flat[]   = "{\"a\":1,\"b\":\"hello\",\"c\":true,\"d\":false,\"e\":null,\"f\":3.14}";
static char s_nested[] = "{\"x\":{\"y\":[1,2,3]},\"z\":42}";
static char s_arr[]    = "[10,20,30]";
static char s_int[]    = "99";
static char s_real[]   = "1.5";
static char s_str[]    = "\"world\"";
static char s_true[]   = "true";
static char s_false[]  = "false";
static char s_null[]   = "null";
static char s_eobj[]   = "{}";
static char s_earr[]   = "[]";

static json_t *flat;
static json_t *nested;
static json_t *arr;
static json_t *j_int;
static json_t *j_real;
static json_t *j_str;
static json_t *j_true;
static json_t *j_false;
static json_t *j_null;
static json_t *j_eobj;
static json_t *j_earr;

/* Null safety — every public function must survive a NULL node */
static void test_null_safety(void)
{
    TEST(json_key(NULL) == NULL);
    TEST(strcmp(json_name(NULL), "") == 0);
    TEST(json_string(NULL) == NULL);
    TEST(strcmp(json_text(NULL), "") == 0);
    TEST(json_number(NULL) == 0.0);
    TEST(json_boolean(NULL) == 0);

    TEST(json_is_iterable(NULL) == 0);
    TEST(json_is_scalar(NULL) == 0);
    TEST(json_is_object(NULL) == 0);
    TEST(json_is_array(NULL) == 0);
    TEST(json_is_string(NULL) == 0);
    TEST(json_is_integer(NULL) == 0);
    TEST(json_is_unsigned(NULL) == 0);
    TEST(json_is_real(NULL) == 0);
    TEST(json_is_number(NULL) == 0);
    TEST(json_is_boolean(NULL) == 0);
    TEST(json_is_true(NULL) == 0);
    TEST(json_is_false(NULL) == 0);
    TEST(json_is_null(NULL) == 0);

    TEST(json_type(NULL) == JSON_UNDEFINED);
    TEST(json_size(NULL) == 0);
    TEST(json_height(NULL) == 0);
    TEST(json_properties(NULL) == 0);
    TEST(json_items(NULL) == 0);

    TEST(json_index(NULL, "a") == JSON_NOT_FOUND);
    TEST(json_child(NULL) == NULL);
    TEST(json_head(NULL) == NULL);
    TEST(json_tail(NULL) == NULL);
    TEST(json_at(NULL, 0) == NULL);
    TEST(json_find(NULL, "a") == NULL);
    TEST(json_locate(NULL, j_int) == NULL);
    TEST(json_locate(arr, NULL) == NULL);

    TEST(json_match(NULL, "*") == 0);
    TEST(json_regex(NULL, ".*") == 0);

    TEST(json_equal(NULL, NULL) == 1);
    TEST(json_equal(NULL, j_int) == 0);
    TEST(json_equal(j_int, NULL) == 0);
    TEST(json_compare(NULL, NULL) == 0);
    TEST(json_compare(NULL, j_int) < 0);
    TEST(json_compare(j_int, NULL) > 0);

    TEST(json_is_unique(NULL, j_int) == 0);
    TEST(json_is_unique(arr, NULL) == 0);
    TEST(json_unique_children(NULL) == 0);

    TEST(json_walk(NULL, NULL, NULL) == 0);
}

/* Type predicates */
static void test_predicates(void)
{
    /* object */
    TEST(json_is_object(flat));
    TEST(json_is_iterable(flat));
    TEST(!json_is_array(flat));
    TEST(!json_is_scalar(flat));

    /* array */
    TEST(json_is_array(arr));
    TEST(json_is_iterable(arr));
    TEST(!json_is_object(arr));

    /* integer */
    TEST(json_is_integer(j_int));
    TEST(json_is_number(j_int));
    TEST(json_is_scalar(j_int));
    TEST(!json_is_real(j_int));
    TEST(!json_is_string(j_int));

    /* real */
    TEST(json_is_real(j_real));
    TEST(json_is_number(j_real));
    TEST(json_is_scalar(j_real));
    TEST(!json_is_integer(j_real));

    /* string */
    TEST(json_is_string(j_str));
    TEST(json_is_scalar(j_str));
    TEST(!json_is_number(j_str));

    /* booleans */
    TEST(json_is_true(j_true));
    TEST(json_is_boolean(j_true));
    TEST(json_is_scalar(j_true));
    TEST(!json_is_false(j_true));

    TEST(json_is_false(j_false));
    TEST(json_is_boolean(j_false));
    TEST(!json_is_true(j_false));

    /* null */
    TEST(json_is_null(j_null));
    TEST(json_is_scalar(j_null));
    TEST(!json_is_boolean(j_null));

    /* empty containers are iterable, not scalar */
    TEST(json_is_object(j_eobj));
    TEST(json_is_array(j_earr));
    TEST(!json_is_scalar(j_eobj));

    /* unsigned: positive integer */
    TEST(json_is_unsigned(j_int));
    char s_neg[] = "-5";
    json_t *neg = json_decode(s_neg);
    if (neg) { TEST(!json_is_unsigned(neg)); free(neg); }
}

/* Accessor functions */
static void test_accessors(void)
{
    /* keys: only set on object values, not on root or array items */
    TEST(json_key(flat) == NULL);
    TEST(strcmp(json_name(flat), "") == 0);
    TEST(strcmp(json_key(json_at(flat, 0)), "a") == 0);
    TEST(strcmp(json_name(json_at(flat, 0)), "a") == 0);
    TEST(json_key(json_at(arr, 0)) == NULL);
    TEST(strcmp(json_name(json_at(arr, 0)), "") == 0);

    /* string accessor */
    TEST(strcmp(json_string(j_str), "world") == 0);
    TEST(strcmp(json_text(j_str), "world") == 0);
    TEST(json_string(j_int) == NULL);
    TEST(strcmp(json_text(j_int), "") == 0);

    /* string child inside flat */
    json_t *b = json_find(flat, "b");
    TEST(b != NULL);
    if (b) { TEST(strcmp(json_string(b), "hello") == 0); }

    /* number accessor */
    TEST(json_number(j_int) == 99.0);
    TEST(fabs(json_number(j_real) - 1.5) < 1e-9);
    TEST(json_number(j_str) == 0.0);

    /* flat's "f" child: 3.14 */
    json_t *f = json_find(flat, "f");
    TEST(f != NULL);
    if (f) { TEST(fabs(json_number(f) - 3.14) < 1e-9); }

    /* boolean: 1 for true, 0 for everything else */
    TEST(json_boolean(j_true) == 1);
    TEST(json_boolean(j_false) == 0);
    TEST(json_boolean(j_null) == 0);
    TEST(json_boolean(j_int) == 0);
}

/* Structure: type, size, height, properties, items */
static void test_structure(void)
{
    TEST(json_type(flat) == JSON_OBJECT);
    TEST(json_type(arr) == JSON_ARRAY);
    TEST(json_type(j_int) == JSON_INTEGER);
    TEST(json_type(j_real) == JSON_REAL);
    TEST(json_type(j_str) == JSON_STRING);
    TEST(json_type(j_true) == JSON_TRUE);
    TEST(json_type(j_false) == JSON_FALSE);
    TEST(json_type(j_null) == JSON_NULL);

    TEST(json_size(flat) == 6);
    TEST(json_size(arr) == 3);
    TEST(json_size(j_int) == 0);
    TEST(json_size(j_eobj) == 0);

    /* height: deepest leaf depth seen by json_walk */
    TEST(json_height(j_int) == 0);   /* scalar: depth 0 only */
    TEST(json_height(j_eobj) == 0);  /* empty: depth 0 only */
    TEST(json_height(arr) == 1);     /* [10,20,30]: leaves at depth 1 */
    TEST(json_height(flat) == 1);    /* flat object: leaves at depth 1 */
    TEST(json_height(nested) == 3);  /* root→x→y→[1,2,3] */

    TEST(json_properties(flat) == 6);
    TEST(json_properties(arr) == 0);
    TEST(json_properties(j_eobj) == 0);

    TEST(json_items(arr) == 3);
    TEST(json_items(flat) == 0);
    TEST(json_items(j_earr) == 0);
}

/* Navigation: child, head, tail, at, index, find, locate */
static void test_navigation(void)
{
    /* child / head return first child */
    TEST(json_child(flat) == json_head(flat));
    TEST(json_child(flat) == json_at(flat, 0));
    TEST(json_child(j_eobj) == NULL);
    TEST(json_head(j_earr) == NULL);

    /* tail returns last child */
    TEST(json_tail(arr) == json_at(arr, 2));
    TEST(json_tail(j_eobj) == NULL);

    /* at: valid and out-of-bounds */
    TEST(json_at(arr, 0) != NULL);
    TEST(json_number(json_at(arr, 0)) == 10);
    TEST(json_number(json_at(arr, 1)) == 20);
    TEST(json_number(json_at(arr, 2)) == 30);
    TEST(json_at(arr, 3) == NULL);
    TEST(json_at(j_int, 0) == NULL);

    /* index: position of key in object */
    TEST(json_index(flat, "a") == 0);
    TEST(json_index(flat, "b") == 1);
    TEST(json_index(flat, "f") == 5);
    TEST(json_index(flat, "z") == JSON_NOT_FOUND);
    TEST(json_index(arr, "a") == JSON_NOT_FOUND);   /* array, not object */
    TEST(json_index(j_int, "a") == JSON_NOT_FOUND); /* scalar */

    /* find: locate child by key */
    json_t *found = json_find(flat, "a");
    TEST(found != NULL);
    if (found) { TEST(json_number(found) == 1); }

    TEST(json_find(flat, "e") != NULL);
    TEST(json_is_null(json_find(flat, "e")));
    TEST(json_find(flat, "z") == NULL);
    TEST(json_find(arr, "a") == NULL);  /* arrays have no keys */

    /* locate: find child equal in value */
    json_t *loc = json_locate(arr, json_at(arr, 0));
    TEST(loc == json_at(arr, 0));

    char sv[] = "20";
    json_t *twenty = json_decode(sv);
    if (twenty)
    {
        TEST(json_locate(arr, twenty) == json_at(arr, 1));
        free(twenty);
    }
    TEST(json_locate(flat, j_int) == NULL); /* 99 not in flat */

    /* navigate nested tree */
    json_t *x = json_find(nested, "x");
    TEST(x != NULL);
    if (x)
    {
        TEST(json_is_object(x));
        json_t *y = json_find(x, "y");
        TEST(y != NULL);
        if (y)
        {
            TEST(json_is_array(y));
            TEST(json_size(y) == 3);
            TEST(json_number(json_at(y, 0)) == 1);
            TEST(json_number(json_at(y, 2)) == 3);
        }
    }

    json_t *z = json_find(nested, "z");
    TEST(z != NULL);
    if (z) { TEST(json_number(z) == 42); }
}

/* Walk callback: counts visited nodes */
static int count_nodes(const json_t *node, size_t depth, void *data)
{
    (void)node; (void)depth;
    (*(int *)data)++;
    return 1;
}

/* Walk callback: stops after N nodes */
static int stop_after_one(const json_t *node, size_t depth, void *data)
{
    (void)node; (void)depth; (void)data;
    return 0; /* stop immediately */
}

static void test_walk(void)
{
    int count;

    /* NULL node or callback */
    TEST(json_walk(NULL, count_nodes, NULL) == 0);
    TEST(json_walk(flat, NULL, NULL) == 0);

    /* scalar: only root visited */
    count = 0;
    json_walk(j_int, count_nodes, &count);
    TEST(count == 1);

    /* empty container: only root visited */
    count = 0;
    json_walk(j_eobj, count_nodes, &count);
    TEST(count == 1);

    /* flat array [10,20,30]: root + 3 children */
    count = 0;
    json_walk(arr, count_nodes, &count);
    TEST(count == 4);

    /* flat object with 6 children: root + 6 */
    count = 0;
    json_walk(flat, count_nodes, &count);
    TEST(count == 7);

    /* nested {x:{y:[1,2,3]},z:42}: root + x + z + y + 1 + 2 + 3 */
    count = 0;
    json_walk(nested, count_nodes, &count);
    TEST(count == 7);

    /* early stop: callback returns 0 on first call */
    count = 0;
    json_walk(flat, stop_after_one, &count);
    TEST(count == 0); /* callback returned 0 before incrementing */
}

/* Deep equality */
static void test_equal(void)
{
    char sa[] = "{\"a\":1,\"b\":\"hello\"}";
    char sb[] = "{\"a\":1,\"b\":\"hello\"}";
    char sc[] = "{\"a\":1,\"b\":\"world\"}";
    char sd[] = "{\"a\":2,\"b\":\"hello\"}";

    json_t *na = json_decode(sa);
    json_t *nb = json_decode(sb);
    json_t *nc = json_decode(sc);
    json_t *nd = json_decode(sd);

    if (na && nb && nc && nd)
    {
        TEST(json_equal(na, nb));          /* same content */
        TEST(!json_equal(na, nc));         /* different string value */
        TEST(!json_equal(na, nd));         /* different number value */
        TEST(!json_equal(na, j_int));      /* different types */
        TEST(json_equal(j_null, j_null));  /* null == null (same pointer) */
    }

    free(na); free(nb); free(nc); free(nd);
}

/* Ordering */
static void test_compare(void)
{
    char s1[] = "1";
    char s2[] = "1";
    char s3[] = "2";
    char sa[] = "\"apple\"";
    char sb[] = "\"banana\"";

    json_t *n1 = json_decode(s1);
    json_t *n2 = json_decode(s2);
    json_t *n3 = json_decode(s3);
    json_t *na = json_decode(sa);
    json_t *nb = json_decode(sb);

    if (n1 && n2 && n3 && na && nb)
    {
        TEST(json_compare(n1, n2) == 0);
        TEST(json_compare(n1, n3) < 0);
        TEST(json_compare(n3, n1) > 0);
        TEST(json_compare(na, nb) < 0);   /* "apple" < "banana" */
        TEST(json_compare(nb, na) > 0);
        TEST(json_compare(na, na) == 0);
    }

    free(n1); free(n2); free(n3); free(na); free(nb);
}

/* Uniqueness checks */
static void test_unique(void)
{
    char sdup[] = "[1,1,2]";
    json_t *dup = json_decode(sdup);

    /* [10,20,30]: all unique */
    TEST(json_unique_children(arr));
    TEST(json_is_unique(arr, json_at(arr, 0)));
    TEST(json_is_unique(arr, json_at(arr, 2)));

    if (dup)
    {
        /* [1,1,2]: first 1 is not unique, 2 is */
        TEST(!json_unique_children(dup));
        TEST(!json_is_unique(dup, json_at(dup, 0)));
        TEST(!json_is_unique(dup, json_at(dup, 1)));
        TEST(json_is_unique(dup, json_at(dup, 2)));
        free(dup);
    }
}

/* Pattern and regex matching on string nodes */
static void test_match(void)
{
    /* json_match validates JSON Schema formats: "date", "email", "ipv4", etc. */
    char s_email[] = "\"user@example.com\"";
    char s_ipv4[]  = "\"192.168.1.1\"";
    char s_date[]  = "\"2024-01-15\"";

    json_t *email = json_decode(s_email);
    json_t *ipv4  = json_decode(s_ipv4);
    json_t *date  = json_decode(s_date);

    if (email && ipv4 && date)
    {
        TEST(json_match(email, "email"));
        TEST(!json_match(email, "ipv4"));
        TEST(!json_match(email, "date"));

        TEST(json_match(ipv4, "ipv4"));
        TEST(!json_match(ipv4, "email"));

        TEST(json_match(date, "date"));
        TEST(!json_match(date, "email"));

        /* unknown format always returns 0 */
        TEST(!json_match(email, "unknown"));
    }

    free(email); free(ipv4); free(date);

    /* regex matching */
    json_t *b = json_find(flat, "b"); /* "hello" */

    TEST(b != NULL);
    if (b)
    {
        TEST(json_regex(b, "^hello$"));
        TEST(json_regex(b, "hel+o"));
        TEST(!json_regex(b, "^world$"));
    }

    /* match/regex on non-string returns 0 */
    TEST(!json_match(j_int, "ipv4"));
    TEST(!json_regex(j_int, ".*"));
}

int main(void)
{
    flat   = json_decode(s_flat);
    nested = json_decode(s_nested);
    arr    = json_decode(s_arr);
    j_int  = json_decode(s_int);
    j_real = json_decode(s_real);
    j_str  = json_decode(s_str);
    j_true  = json_decode(s_true);
    j_false = json_decode(s_false);
    j_null  = json_decode(s_null);
    j_eobj  = json_decode(s_eobj);
    j_earr  = json_decode(s_earr);

    if (!flat || !nested || !arr || !j_int || !j_real || !j_str
     || !j_true || !j_false || !j_null || !j_eobj || !j_earr)
    {
        fprintf(stderr, "fixture setup failed\n");
        return 1;
    }

    test_null_safety();
    test_predicates();
    test_accessors();
    test_structure();
    test_navigation();
    test_walk();
    test_equal();
    test_compare();
    test_unique();
    test_match();

    free(flat);
    free(nested);
    free(arr);
    free(j_int);
    free(j_real);
    free(j_str);
    free(j_true);
    free(j_false);
    free(j_null);
    free(j_eobj);
    free(j_earr);

    TEST_SUMMARY();
}
