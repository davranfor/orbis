/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "json_writer.h"
#include "json_buffer.h"

/* json_encode / json_stringify: compact and indented serialisation */
static void test_encode(void)
{
    /* NULL safety */
    TEST(json_encode(NULL, 0) == NULL);
    TEST(json_stringify(NULL) == NULL);

    /* Compact round-trip: stringify(decode(s)) == canonical form of s */
    char s1[] = "{\"a\":1,\"b\":[2,3]}";
    json_t *n = json_decode(s1);
    TEST(n != NULL);
    if (n)
    {
        char *out = json_stringify(n);
        TEST(out != NULL);
        if (out) { TEST(strcmp(out, "{\"a\":1,\"b\":[2,3]}") == 0); free(out); }
        free(n);
    }

    /* Indented form contains newlines */
    char s2[] = "[1,2]";
    n = json_decode(s2);
    TEST(n != NULL);
    if (n)
    {
        char *out = json_encode(n, 2);
        TEST(out != NULL);
        if (out) { TEST(strchr(out, '\n') != NULL); free(out); }
        free(n);
    }

    /* Empty containers */
    char s3[] = "{}";
    n = json_decode(s3);
    TEST(n != NULL);
    if (n)
    {
        char *out = json_stringify(n);
        TEST(out != NULL);
        if (out) { TEST(strcmp(out, "{}") == 0); free(out); }
        free(n);
    }
}

/* json_quote: escape a raw C string into a JSON string literal */
static void test_quote(void)
{
    TEST(json_quote(NULL) == NULL);

    char *out = json_quote("hello");
    TEST(out != NULL);
    if (out) { TEST(strcmp(out, "\"hello\"") == 0); free(out); }

    /* Escapes: " and \ must be escaped */
    out = json_quote("a\"b");
    TEST(out != NULL);
    if (out) { TEST(strcmp(out, "\"a\\\"b\"") == 0); free(out); }

    out = json_quote("c\\d");
    TEST(out != NULL);
    if (out) { TEST(strcmp(out, "\"c\\\\d\"") == 0); free(out); }

    /* Control characters: newline */
    out = json_quote("a\nb");
    TEST(out != NULL);
    if (out) { TEST(strcmp(out, "\"a\\nb\"") == 0); free(out); }
}

/* json_convert: render a number as JSON literal */
static void test_convert(void)
{
    char *out = json_convert(42, JSON_INTEGER);
    TEST(out != NULL);
    if (out) { TEST(strcmp(out, "42") == 0); free(out); }

    out = json_convert(-7, JSON_INTEGER);
    TEST(out != NULL);
    if (out) { TEST(strcmp(out, "-7") == 0); free(out); }

    /* JSON_REAL always emits decimal form (e.g. "1.5") */
    out = json_convert(1.5, JSON_REAL);
    TEST(out != NULL);
    if (out) { TEST(strchr(out, '.') != NULL); free(out); }

    /* Integer-valued REAL still gets the ".0" suffix */
    out = json_convert(3, JSON_REAL);
    TEST(out != NULL);
    if (out) { TEST(strcmp(out, "3.0") == 0); free(out); }
}

/* Encoding mode toggle: UTF8 (default) vs ASCII */
static void test_encoding(void)
{
    TEST(json_get_encoding() == JSON_UTF8);

    json_set_encoding(JSON_ASCII);
    TEST(json_get_encoding() == JSON_ASCII);

    json_set_encoding(JSON_UTF8);
    TEST(json_get_encoding() == JSON_UTF8);
}

int main(void)
{
    test_encode();
    test_quote();
    test_convert();
    test_encoding();

    TEST_SUMMARY();
}
