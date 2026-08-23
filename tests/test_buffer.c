/*!
 *  \brief     C library for unixes
 *  \author    David Ranieri <davranfor@gmail.com>
 *  \copyright GNU General Public License v3 or later.
 */

#include <stdlib.h>
#include <string.h>
#include "test.h"
#include "clib_buffer.h"

/* buffer_append / buffer_write: grow a buffer and keep it NUL-terminated */
static void test_append_write(void)
{
    buffer_t buffer = { 0 };

    TEST(buffer_write(&buffer, "hello") != NULL);
    TEST(buffer.length == 5);
    TEST(strcmp(buffer.text, "hello") == 0);

    TEST(buffer_append(&buffer, " world", 6) != NULL);
    TEST(buffer.length == 11);
    TEST(strcmp(buffer.text, "hello world") == 0);

    buffer_clear(&buffer);
}

/* buffer_insert: at the start, in the middle, at the end, into an empty
   buffer, and with an out-of-range index (clamped to buffer->length) */
static void test_insert(void)
{
    buffer_t buffer = { 0 };

    /* Insert into an empty buffer */
    TEST(buffer_insert(&buffer, 0, "world", 5) != NULL);
    TEST(buffer.length == 5);
    TEST(strcmp(buffer.text, "world") == 0);

    /* Insert at the start */
    TEST(buffer_insert(&buffer, 0, "hello ", 6) != NULL);
    TEST(buffer.length == 11);
    TEST(strcmp(buffer.text, "hello world") == 0);

    /* Insert in the middle */
    TEST(buffer_insert(&buffer, 5, " cruel", 6) != NULL);
    TEST(buffer.length == 17);
    TEST(strcmp(buffer.text, "hello cruel world") == 0);

    /* Insert at the end (index == length) behaves like append */
    TEST(buffer_insert(&buffer, buffer.length, "!", 1) != NULL);
    TEST(buffer.length == 18);
    TEST(strcmp(buffer.text, "hello cruel world!") == 0);

    /* Out-of-range index is clamped to buffer->length */
    TEST(buffer_insert(&buffer, 999, "?", 1) != NULL);
    TEST(buffer.length == 19);
    TEST(strcmp(buffer.text, "hello cruel world!?") == 0);

    buffer_clear(&buffer);
}

/* buffer_delete: remove a slice, and no-op on an out-of-range request */
static void test_delete(void)
{
    buffer_t buffer = { 0 };

    buffer_write(&buffer, "hello cruel world");

    TEST(buffer_delete(&buffer, 5, 6) != NULL);
    TEST(buffer.length == 11);
    TEST(strcmp(buffer.text, "hello world") == 0);

    /* index + length beyond buffer->length: no-op */
    TEST(buffer_delete(&buffer, 5, 999) != NULL);
    TEST(buffer.length == 11);
    TEST(strcmp(buffer.text, "hello world") == 0);

    /* Deleting from an empty buffer is also a no-op: buffer->text is
       NULL after buffer_clear(), and that's what's returned */
    buffer_clear(&buffer);
    TEST(buffer_delete(&buffer, 0, 1) == NULL);
    TEST(buffer.length == 0);

    buffer_clear(&buffer);
}

/* buffer_put / buffer_repeat */
static void test_put_repeat(void)
{
    buffer_t buffer = { 0 };

    TEST(buffer_put(&buffer, 'a') != NULL);
    TEST(buffer_put(&buffer, 'b') != NULL);
    TEST(buffer.length == 2);
    TEST(strcmp(buffer.text, "ab") == 0);

    TEST(buffer_repeat(&buffer, '-', 4) != NULL);
    TEST(buffer.length == 6);
    TEST(strcmp(buffer.text, "ab----") == 0);

    buffer_clear(&buffer);
}

/* buffer_format: printf-style, growable, appends to existing content */
static void test_format(void)
{
    buffer_t buffer = { 0 };

    TEST(buffer_format(&buffer, "%d-%s", 42, "answer") != NULL);
    TEST(strcmp(buffer.text, "42-answer") == 0);

    TEST(buffer_format(&buffer, "/%d", 7) != NULL);
    TEST(strcmp(buffer.text, "42-answer/7") == 0);

    buffer_clear(&buffer);
}

/* buffer_truncate: plain ASCII, and stepping back over a split UTF-8
   sequence instead of cutting it in half */
static void test_truncate(void)
{
    buffer_t buffer = { 0 };

    buffer_write(&buffer, "hello");
    TEST(buffer_truncate(&buffer, 3) != NULL);
    TEST(buffer.length == 3);
    TEST(strcmp(buffer.text, "hel") == 0);

    buffer_clear(&buffer);

    /* "a" + U+00E9 ('\xc3\xa9'): truncating at 2 lands on the trailing
       byte of the 2-byte sequence, so it must back off to length 1 */
    buffer_write(&buffer, "a\xc3\xa9");
    TEST(buffer.length == 3);
    TEST(buffer_truncate(&buffer, 2) != NULL);
    TEST(buffer.length == 1);
    TEST(strcmp(buffer.text, "a") == 0);

    buffer_clear(&buffer);
}

/* buffer->error is sticky: once set, every write becomes a no-op until
   buffer_reset()/buffer_clear() runs */
static void test_error(void)
{
    buffer_t buffer = { 0 };

    buffer_write(&buffer, "kept");
    buffer_set_error(&buffer, BUFFER_ERROR_RESIZE);

    TEST(buffer.error == BUFFER_ERROR_RESIZE);
    TEST(buffer.text == NULL);
    TEST(buffer.length == 0);

    /* Every write function is now a no-op */
    TEST(buffer_write(&buffer, "more") == NULL);
    TEST(buffer_append(&buffer, "more", 4) == NULL);
    TEST(buffer_format(&buffer, "%d", 1) == NULL);
    TEST(buffer.length == 0);

    /* buffer_reset() clears the error and writes work again */
    buffer_reset(&buffer);
    TEST(buffer.error == 0);
    TEST(buffer_write(&buffer, "back") != NULL);
    TEST(strcmp(buffer.text, "back") == 0);

    buffer_clear(&buffer);
}

/* buffer_create / buffer_destroy / buffer_free: heap-allocated buffer_t */
static void test_create_destroy(void)
{
    buffer_t *buffer = buffer_create();

    TEST(buffer != NULL);
    if (buffer)
    {
        TEST(buffer->text == NULL);
        TEST(buffer->length == 0);
        TEST(buffer_write(buffer, "heap") != NULL);
        TEST(strcmp(buffer->text, "heap") == 0);
        buffer_destroy(buffer);
    }

    buffer_t *other = buffer_create();

    TEST(other != NULL);
    if (other)
    {
        buffer_write(other, "free");
        buffer_free(other); /* void * signature: usable as a map/list destructor */
    }
}

int main(void)
{
    test_append_write();
    test_insert();
    test_delete();
    test_put_repeat();
    test_format();
    test_truncate();
    test_error();
    test_create_destroy();

    TEST_SUMMARY();
}
