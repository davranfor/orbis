/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "json_parser.h"

/* Collector: records event types and depths emitted by json_parse */
typedef struct
{
    unsigned type[64];
    unsigned depth[64];
    char    *key[64];
    int      count;
    int      stop_after; /* return 0 after this many events (0 = never) */
} events_t;

static int collect(const json_event_t *event)
{
    events_t *ev = event->cookie;

    if (ev->count < 64)
    {
        ev->type[ev->count]  = event->type;
        ev->depth[ev->count] = event->depth;
        ev->key[ev->count]   = event->key;
        ev->count++;
    }
    if (ev->stop_after > 0 && ev->count >= ev->stop_after)
    {
        return 0;
    }
    return 1;
}

/* NULL and invalid input */
static void test_invalid(void)
{
    events_t ev = { 0 };

    TEST(json_parse(NULL, collect, &ev) == 0);

    char empty[] = "";
    TEST(json_parse(empty, collect, &ev) == 0);

    char bad1[] = "{";
    TEST(json_parse(bad1, collect, &ev) == 0);

    char bad2[] = "[1, 2,]";
    TEST(json_parse(bad2, collect, &ev) == 0);

    char bad3[] = "{\"a\"}";
    TEST(json_parse(bad3, collect, &ev) == 0);
}

/* Scalar values produce exactly one event each */
static void test_scalars(void)
{
    struct { char str[32]; unsigned type; } cases[] = {
        { "42",      JSON_INTEGER },
        { "3.14",    JSON_REAL    },
        { "\"hi\"",  JSON_STRING  },
        { "true",    JSON_TRUE    },
        { "false",   JSON_FALSE   },
        { "null",    JSON_NULL    },
    };

    for (int i = 0; i < 6; i++)
    {
        events_t ev = { 0 };
        char buf[32];
        strcpy(buf, cases[i].str);

        TEST(json_parse(buf, collect, &ev) == 1);
        TEST(ev.count == 1);
        TEST(ev.type[0] == cases[i].type);
        TEST(ev.depth[0] == 0);
        TEST(ev.key[0] == NULL);
    }
}

/* Empty containers */
static void test_empty_containers(void)
{
    events_t ev = { 0 };

    char s1[] = "{}";
    TEST(json_parse(s1, collect, &ev) == 1);
    TEST(ev.count == 2);
    TEST(ev.type[0] == JSON_OBJECT);     TEST(ev.depth[0] == 0);
    TEST(ev.type[1] == JSON_OBJECT_END); TEST(ev.depth[1] == 0);

    ev = (events_t){ 0 };
    char s2[] = "[]";
    TEST(json_parse(s2, collect, &ev) == 1);
    TEST(ev.count == 2);
    TEST(ev.type[0] == JSON_ARRAY);     TEST(ev.depth[0] == 0);
    TEST(ev.type[1] == JSON_ARRAY_END); TEST(ev.depth[1] == 0);
}

/* Flat object: event order, depths, and keys */
static void test_flat_object(void)
{
    events_t ev = { 0 };
    char s[] = "{\"a\": 1, \"b\": true}";

    TEST(json_parse(s, collect, &ev) == 1);
    /* Expected: OBJECT(0) INTEGER(1) TRUE(1) OBJECT_END(0) */
    TEST(ev.count == 4);
    TEST(ev.type[0] == JSON_OBJECT);     TEST(ev.depth[0] == 0);
    TEST(ev.type[1] == JSON_INTEGER);    TEST(ev.depth[1] == 1); TEST(strcmp(ev.key[1], "a") == 0);
    TEST(ev.type[2] == JSON_TRUE);       TEST(ev.depth[2] == 1); TEST(strcmp(ev.key[2], "b") == 0);
    TEST(ev.type[3] == JSON_OBJECT_END); TEST(ev.depth[3] == 0);
}

/* Flat array: event order and depths */
static void test_flat_array(void)
{
    events_t ev = { 0 };
    char s[] = "[1, 2, 3]";

    TEST(json_parse(s, collect, &ev) == 1);
    /* Expected: ARRAY(0) INTEGER(1) INTEGER(1) INTEGER(1) ARRAY_END(0) */
    TEST(ev.count == 5);
    TEST(ev.type[0] == JSON_ARRAY);    TEST(ev.depth[0] == 0);
    TEST(ev.type[1] == JSON_INTEGER);  TEST(ev.depth[1] == 1);
    TEST(ev.type[2] == JSON_INTEGER);  TEST(ev.depth[2] == 1);
    TEST(ev.type[3] == JSON_INTEGER);  TEST(ev.depth[3] == 1);
    TEST(ev.type[4] == JSON_ARRAY_END); TEST(ev.depth[4] == 0);
}

/* Nested: depths increase and decrease correctly */
static void test_nested_depths(void)
{
    events_t ev = { 0 };
    char s[] = "{\"a\": {\"b\": 1}}";

    TEST(json_parse(s, collect, &ev) == 1);
    /* OBJECT(0) OBJECT(1) INTEGER(2) OBJECT_END(1) OBJECT_END(0) */
    TEST(ev.count == 5);
    TEST(ev.depth[0] == 0); /* outer object */
    TEST(ev.depth[1] == 1); /* inner object, key "a" */
    TEST(ev.depth[2] == 2); /* integer 1, key "b" */
    TEST(ev.depth[3] == 1); /* inner object end */
    TEST(ev.depth[4] == 0); /* outer object end */
    TEST(strcmp(ev.key[1], "a") == 0);
    TEST(strcmp(ev.key[2], "b") == 0);
}

/* Callback returning 0 aborts parsing */
static void test_early_stop(void)
{
    events_t ev = { .stop_after = 1 };
    char s[] = "[1, 2, 3]";

    /* Callback stops after 1 event → parse returns 0 */
    TEST(json_parse(s, collect, &ev) == 0);
    TEST(ev.count == 1);
}

int main(void)
{
    test_invalid();
    test_scalars();
    test_empty_containers();
    test_flat_object();
    test_flat_array();
    test_nested_depths();
    test_early_stop();

    TEST_SUMMARY();
}
