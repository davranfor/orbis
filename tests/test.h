/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU Public License.
 */

#ifndef TEST_H
#define TEST_H

#include <stdio.h>

static int test_count = 0;
static int test_fails = 0;

#define TEST(expr) do {                                             \
    test_count++;                                                   \
    if (!(expr)) {                                                  \
        fprintf(stderr, "  FAIL line %d: %s\n", __LINE__, #expr);  \
        test_fails++;                                               \
    }                                                               \
} while (0)

#define TEST_SUMMARY()                                              \
    printf("%s: %d/%d passed\n",                                    \
        __FILE__, test_count - test_fails, test_count);             \
    return test_fails > 0

#endif
