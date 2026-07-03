/* stream_tests.c — Stream lifecycle tests for Commit 2
 *
 * Tests:
 *   - gli_new_stream() allocation and initialisation
 *   - gli_delete_stream() destruction
 *   - glk_stream_iterate() iteration
 *   - linked-list insertion (head of gli_streamlist)
 *   - linked-list removal (unlinking)
 *   - empty-list behaviour
 *   - current-stream cleanup on delete
 */

#include "../nextglk/nextglk_internal.h"
#include "test_common.h"
#include <string.h>

int stream_tests_run(void)
{
    stream_t *s1, *s2, *s3;
    stream_t *iter;
    int count;

    /* ---- Initial state: empty list ---- */

    TEST_ASSERT(gli_streamlist == NULL,
        "gli_streamlist is NULL initially");

    iter = glk_stream_iterate(NULL, NULL);
    TEST_ASSERT(iter == NULL,
        "glk_stream_iterate(NULL) returns NULL on empty list");

    /* ---- Allocate stream 1 ---- */

    s1 = gli_new_stream(strtype_Window, 0, 1, 0);
    TEST_ASSERT(s1 != NULL, "gli_new_stream(strtype_Window) returns non-NULL");
    TEST_ASSERT(gli_streamlist == s1,
        "after alloc, gli_streamlist points to new stream");
    TEST_ASSERT(s1->type == strtype_Window,
        "stream type == strtype_Window");
    TEST_ASSERT(s1->readable == 0,
        "stream readable == 0");
    TEST_ASSERT(s1->writable == 1,
        "stream writable == 1");
    TEST_ASSERT(s1->readcount == 0,
        "stream readcount == 0");
    TEST_ASSERT(s1->writecount == 0,
        "stream writecount == 0");
    TEST_ASSERT(s1->prev == NULL,
        "first stream prev == NULL");
    TEST_ASSERT(s1->next == NULL,
        "first stream next == NULL");
    TEST_ASSERT(s1->win == NULL,
        "stream win == NULL (not yet associated with window)");

    /* ---- Allocate stream 2: insert at head ---- */

    s2 = gli_new_stream(strtype_File, 1, 0, 0);
    TEST_ASSERT(s2 != NULL, "gli_new_stream(strtype_File) returns non-NULL");
    TEST_ASSERT(gli_streamlist == s2,
        "after second alloc, gli_streamlist points to newest stream");
    TEST_ASSERT(s2->prev == NULL,
        "newest stream prev == NULL (head of list)");
    TEST_ASSERT(s2->next == s1,
        "newest stream next == s1");
    TEST_ASSERT(s1->prev == s2,
        "s1 prev updated to point to s2");
    TEST_ASSERT(s1->next == NULL,
        "s1 next still NULL (tail of list)");

    /* ---- Allocate stream 3 ---- */

    s3 = gli_new_stream(strtype_Memory, 1, 1, 0);
    TEST_ASSERT(s3 != NULL, "gli_new_stream(strtype_Memory) returns non-NULL");
    TEST_ASSERT(gli_streamlist == s3, "after third alloc, gli_streamlist == s3");
    TEST_ASSERT(s3->prev == NULL, "s3 prev == NULL (head)");
    TEST_ASSERT(s3->next == s2, "s3 next == s2");
    TEST_ASSERT(s2->prev == s3, "s2 prev == s3");
    TEST_ASSERT(s1->prev == s2, "s1 prev still == s2 (s2 not head)");

    /* ---- Iterate over list (forward) ---- */

    count = 0;
    iter = glk_stream_iterate(NULL, NULL);
    while (iter) {
        count++;
        iter = glk_stream_iterate(iter, NULL);
    }
    TEST_ASSERT(count == 3,
        "glk_stream_iterate() yields 3 streams");

    /* Verify iteration order: s3 -> s2 -> s1 */
    iter = glk_stream_iterate(NULL, NULL);
    TEST_ASSERT(iter == s3,
        "first iterated stream is s3 (newest = head)");
    iter = glk_stream_iterate(iter, NULL);
    TEST_ASSERT(iter == s2,
        "second iterated stream is s2");
    iter = glk_stream_iterate(iter, NULL);
    TEST_ASSERT(iter == s1,
        "third iterated stream is s1 (oldest = tail)");
    iter = glk_stream_iterate(iter, NULL);
    TEST_ASSERT(iter == NULL,
        "fourth iteration returns NULL (end of list)");

    /* ---- glk_stream_iterate with rockptr ---- */

    {
        glui32 rock;
        iter = glk_stream_iterate(NULL, &rock);
        TEST_ASSERT(iter == s3,
            "glk_stream_iterate(NULL, &rock) returns s3");
        TEST_ASSERT(rock == 0,
            "rock value is 0 (stub)");
    }

    /* ---- Delete middle stream (s2) ---- */

    gli_delete_stream(s2, NULL);
    TEST_ASSERT(gli_streamlist == s3, "after delete s2, head is still s3");
    TEST_ASSERT(s3->prev == NULL, "s3 prev == NULL (still head)");
    TEST_ASSERT(s3->next == s1, "s3 next == s1 (skipped s2)");
    TEST_ASSERT(s1->prev == s3, "s1 prev == s3 (skipped s2)");
    TEST_ASSERT(s1->next == NULL, "s1 next == NULL (tail)");

    /* Verify list now has 2 elements */
    count = 0;
    iter = glk_stream_iterate(NULL, NULL);
    while (iter) {
        count++;
        iter = glk_stream_iterate(iter, NULL);
    }
    TEST_ASSERT(count == 2,
        "after delete, iteration yields 2 streams");

    /* ---- Delete head (s3) ---- */

    gli_delete_stream(s3, NULL);
    TEST_ASSERT(gli_streamlist == s1,
        "after delete s3, gli_streamlist == s1");
    TEST_ASSERT(s1->prev == NULL,
        "s1 prev == NULL (now head)");
    TEST_ASSERT(s1->next == NULL,
        "s1 next == NULL (only element)");

    /* ---- Delete tail (s1) — list should be empty ---- */

    gli_delete_stream(s1, NULL);
    TEST_ASSERT(gli_streamlist == NULL,
        "after deleting all streams, gli_streamlist == NULL");

    /* ---- Empty-list iteration again ---- */

    iter = glk_stream_iterate(NULL, NULL);
    TEST_ASSERT(iter == NULL,
        "empty list iteration returns NULL");

    /* ---- Current-stream cleanup ---- */

    {
        stream_t *st = gli_new_stream(strtype_Window, 0, 1, 0);
        TEST_ASSERT(st != NULL, "created stream for current-stream test");
        gli_currentstr = st;
        TEST_ASSERT(gli_currentstr == st,
            "gli_currentstr set to test stream");
        gli_delete_stream(st, NULL);
        TEST_ASSERT(gli_currentstr == NULL,
            "gli_currentstr cleared after deleting current stream");
    }

    /* ---- gli_stream_fill_result with NULL result (no crash) ---- */

    {
        stream_t *st = gli_new_stream(strtype_Window, 0, 1, 0);
        TEST_ASSERT(st != NULL, "created stream for fill_result test");
        st->readcount = 10;
        st->writecount = 20;
        gli_stream_fill_result(st, NULL);
        TEST_ASSERT(1, "gli_stream_fill_result with NULL does not crash");
        gli_delete_stream(st, NULL);
    }

    /* ---- gli_stream_fill_result with non-NULL result ---- */

    {
        stream_result_t res;
        stream_t *st = gli_new_stream(strtype_Window, 0, 1, 0);
        TEST_ASSERT(st != NULL, "created stream for fill_result non-NULL test");
        st->readcount = 10;
        st->writecount = 20;
        gli_stream_fill_result(st, &res);
        TEST_ASSERT(res.readcount == 10,
            "result readcount == 10");
        TEST_ASSERT(res.writecount == 20,
            "result writecount == 20");
        gli_delete_stream(st, NULL);
    }

    return 0;
}