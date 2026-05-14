/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test.h"
#include "json_writer.h"
#include "json_struct.h"

/*
 * NOTE: json_struct.c emits unconditional printf() trace lines on every
 * evaluation step. We mute stdout during the tests so only the summary
 * (also stdout) remains visible.
 *
 * IMPORTANT: json_compile() parses its argument in-place and the resulting
 * bytecode keeps pointers into that buffer (property names, patterns, ...).
 * Every schema must therefore live in a buffer that outlives the bytecode —
 * we use a writable local 'char schema[] = "..."' in each test.
 */
static int saved_stdout = -1;
static int devnull_fd   = -1;

static void mute_stdout(void)
{
    fflush(stdout);
    saved_stdout = dup(STDOUT_FILENO);
    devnull_fd   = open("/dev/null", O_WRONLY);
    if (devnull_fd >= 0)
    {
        dup2(devnull_fd, STDOUT_FILENO);
    }
}

static void unmute_stdout(void)
{
    fflush(stdout);
    if (saved_stdout >= 0)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        saved_stdout = -1;
    }
    if (devnull_fd >= 0)
    {
        close(devnull_fd);
        devnull_fd = -1;
    }
}

/* Decode (writable copy) + validate + free, returns 1/0 or -1 on decode error */
static int validate_str(const char *src, const void *code)
{
    char buf[256];

    strncpy(buf, src, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    json_t *node = json_decode(buf);
    if (node == NULL)
    {
        return -1;
    }
    int rc = json_validate(node, code, NULL, NULL);
    free(node);
    return rc;
}

/* NULL safety */
static void test_null_safety(void)
{
    TEST(json_compile(NULL) == NULL);

    char schema[] = "(string)";
    void *code = json_compile(schema);
    TEST(code != NULL);

    TEST(json_validate(NULL, code, NULL, NULL) == 0);

    char s[] = "\"x\"";
    json_t *node = json_decode(s);
    TEST(node != NULL);
    if (node)
    {
        TEST(json_validate(node, NULL, NULL, NULL) == 0);
        free(node);
    }
    free(code);
}

/* Malformed schemas must fail to compile */
static void test_compile_invalid(void)
{
    char bad1[] = "(unknown)";
    TEST(json_compile(bad1) == NULL);

    char bad2[] = "(property \"x\" (string))";  /* property outside object */
    TEST(json_compile(bad2) == NULL);

    char bad3[] = "(object";  /* unbalanced */
    TEST(json_compile(bad3) == NULL);
}

/* Primitive type validators */
static void test_primitives(void)
{
    struct { char schema[32]; const char *good; const char *bad; } cases[] =
    {
        { "(string)",  "\"hi\"", "42"    },
        { "(integer)", "7",      "\"x\"" },
        { "(number)",  "3.14",   "\"x\"" },
        { "(boolean)", "true",   "42"    },
        { "(null)",    "null",   "0"     },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        void *code = json_compile(cases[i].schema);
        TEST(code != NULL);
        if (code)
        {
            TEST(validate_str(cases[i].good, code) == 1);
            TEST(validate_str(cases[i].bad,  code) == 0);
            free(code);
        }
    }
}

/* Object with required and optional properties */
static void test_object(void)
{
    char schema[] =
        "(object"
        "  (property \"name\" (string))"
        "  (property \"age\" (optional) (integer)))";
    void *code = json_compile(schema);
    TEST(code != NULL);
    if (!code) return;

    TEST(validate_str("{\"name\":\"Alice\",\"age\":30}", code) == 1);
    TEST(validate_str("{\"name\":\"Alice\"}",            code) == 1);   /* optional missing */
    TEST(validate_str("{\"age\":30}",                    code) == 0);   /* required missing */
    TEST(validate_str("{\"name\":42}",                   code) == 0);   /* wrong type */

    free(code);
}

/* Array of T with size bounds */
static void test_array(void)
{
    char schema[] = "(array (minItems 1) (maxItems 3) (integer))";
    void *code = json_compile(schema);
    TEST(code != NULL);
    if (!code) return;

    TEST(validate_str("[1]",       code) == 1);
    TEST(validate_str("[1,2,3]",   code) == 1);
    TEST(validate_str("[]",        code) == 0);   /* below minItems */
    TEST(validate_str("[1,2,3,4]", code) == 0);   /* above maxItems */
    TEST(validate_str("[1,\"x\"]", code) == 0);   /* wrong element type */

    free(code);
}

/* Tuple: positional, fixed-size, heterogeneous */
static void test_tuple(void)
{
    char schema[] = "(tuple (string) (integer))";
    void *code = json_compile(schema);
    TEST(code != NULL);
    if (!code) return;

    TEST(validate_str("[\"x\",1]", code) == 1);
    TEST(validate_str("[1,\"x\"]", code) == 0);   /* swapped types */

    free(code);
}

/* String constraints: minLength, maxLength */
static void test_string_constraints(void)
{
    char schema[] = "(string (minLength 2) (maxLength 4))";
    void *code = json_compile(schema);
    TEST(code != NULL);
    if (!code) return;

    TEST(validate_str("\"ab\"",    code) == 1);
    TEST(validate_str("\"abcd\"",  code) == 1);
    TEST(validate_str("\"a\"",     code) == 0);
    TEST(validate_str("\"abcde\"", code) == 0);

    free(code);
}

/* Number constraints: min, max, multipleOf */
static void test_number_constraints(void)
{
    char schema[] = "(integer (min 0) (max 10) (multipleOf 2))";
    void *code = json_compile(schema);
    TEST(code != NULL);
    if (!code) return;

    TEST(validate_str("0",  code) == 1);
    TEST(validate_str("4",  code) == 1);
    TEST(validate_str("10", code) == 1);
    TEST(validate_str("-2", code) == 0);   /* below min */
    TEST(validate_str("12", code) == 0);   /* above max */
    TEST(validate_str("3",  code) == 0);   /* not a multiple of 2 */

    free(code);
}

int main(void)
{
    mute_stdout();

    test_null_safety();
    test_compile_invalid();
    test_primitives();
    test_object();
    test_array();
    test_tuple();
    test_string_constraints();
    test_number_constraints();

    unmute_stdout();

    TEST_SUMMARY();
}
