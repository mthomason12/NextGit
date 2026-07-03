/* stream_output_tests.c — Stream output tests for Commit 4
 *
 * Tests:
 *   - glk_stream_get_current() returns NULL initially
 *   - glk_stream_set_current() sets the current stream
 *   - glk_stream_get_current() returns the set stream
 *   - glk_stream_set_current(NULL) clears the current stream
 *   - Window stream output routes through nextglk_put_char()
 *   - glk_put_string_stream() writes each character
 *   - glk_put_buffer_stream() writes each byte
 *   - glk_set_style_stream() is safe to call (no crash)
 *   - writecount is incremented on each output call
 */

#include "../nextglk/nextglk_internal.h"
#include "../nextglk/nextglk.h"
#include "test_common.h"
#include <string.h>

/* Reset the platform-layer test counters to zero */
static void reset_counters(void)
{
    nextglk_put_char_count = 0;
    nextglk_put_string_count = 0;
    nextglk_put_buffer_count = 0;
}

int stream_output_tests_run(void)
{
    winid_t win;
    strid_t str;

    /* ---- Initial state: no current stream ---- */

    TEST_ASSERT(glk_stream_get_current() == NULL,
        "glk_stream_get_current() returns NULL initially");

    /* ---- glk_stream_set_current / glk_stream_get_current ---- */

    {
        /* Create a standalone stream (not associated with a window) */
        stream_t *s = gli_new_stream(strtype_Memory, 0, 1, 0);
        TEST_ASSERT(s != NULL, "created memory stream for current-stream test");

        /* Set as current */
        glk_stream_set_current((strid_t)s);
        TEST_ASSERT(glk_stream_get_current() == (strid_t)s,
            "glk_stream_get_current() returns the stream set by glk_stream_set_current()");

        /* Clear current stream */
        glk_stream_set_current(NULL);
        TEST_ASSERT(glk_stream_get_current() == NULL,
            "glk_stream_get_current() returns NULL after glk_stream_set_current(NULL)");

        /* Set again */
        glk_stream_set_current((strid_t)s);
        TEST_ASSERT(glk_stream_get_current() == (strid_t)s,
            "glk_stream_get_current() returns stream after re-setting");

        /* Clean up */
        glk_stream_set_current(NULL);
        gli_delete_stream(s, NULL);
    }

    /* ---- Window stream output routing ---- */

    {
        stream_t *s;
        stream_t *iter;
        int found;

        /* Open a window — this creates a window stream with win back-pointer */
        win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
        TEST_ASSERT(win != NULL, "glk_window_open() succeeds for output test");

        /* Get the window's stream */
        str = glk_window_get_stream(win);
        TEST_ASSERT(str != NULL, "window stream is non-NULL");
        s = (stream_t *)str;
        TEST_ASSERT(s->win != NULL, "window stream has valid win back-pointer");
        TEST_ASSERT(s->type == strtype_Window, "window stream type == strtype_Window");

        /* Reset counters before output test */
        reset_counters();

        /* ---- glk_put_char_stream routes to nextglk_put_char ---- */

        glk_put_char_stream(str, 'A');
        TEST_ASSERT(nextglk_put_char_count == 1,
            "glk_put_char_stream('A') increments nextglk_put_char_count to 1");
        TEST_ASSERT(s->writecount == 1,
            "stream writecount incremented to 1 after glk_put_char_stream");

        glk_put_char_stream(str, 'B');
        TEST_ASSERT(nextglk_put_char_count == 2,
            "glk_put_char_stream('B') increments nextglk_put_char_count to 2");
        TEST_ASSERT(s->writecount == 2,
            "stream writecount incremented to 2 after second glk_put_char_stream");

        /* ---- glk_put_string_stream routes to nextglk_put_char ---- */

        reset_counters();
        s->writecount = 0;

        glk_put_string_stream(str, "Hello");
        TEST_ASSERT(nextglk_put_char_count == 5,
            "glk_put_string_stream(\"Hello\") calls nextglk_put_char 5 times");
        TEST_ASSERT(s->writecount == 5,
            "stream writecount incremented to 5 after glk_put_string_stream");

        /* ---- glk_put_buffer_stream routes to nextglk_put_char ---- */

        reset_counters();
        s->writecount = 0;

        glk_put_buffer_stream(str, "AB", 2);
        TEST_ASSERT(nextglk_put_char_count == 2,
            "glk_put_buffer_stream(\"AB\", 2) calls nextglk_put_char 2 times");
        TEST_ASSERT(s->writecount == 2,
            "stream writecount incremented to 2 after glk_put_buffer_stream");

        /* ---- glk_put_buffer_stream with longer buffer ---- */

        reset_counters();
        s->writecount = 0;

        glk_put_buffer_stream(str, "HelloWorld", 10);
        TEST_ASSERT(nextglk_put_char_count == 10,
            "glk_put_buffer_stream(\"HelloWorld\", 10) calls nextglk_put_char 10 times");
        TEST_ASSERT(s->writecount == 10,
            "stream writecount incremented to 10 after glk_put_buffer_stream");

        /* ---- glk_set_style_stream is safe to call ---- */

        glk_set_style_stream(str, 0);
        TEST_ASSERT(1, "glk_set_style_stream(0) does not crash");

        glk_set_style_stream(str, 1);
        TEST_ASSERT(1, "glk_set_style_stream(1) does not crash");

        glk_set_style_stream(str, 999);
        TEST_ASSERT(1, "glk_set_style_stream(999) does not crash");

        /* ---- glk_put_char_stream with NULL stream (no crash) ---- */

        glk_put_char_stream(NULL, 'X');
        TEST_ASSERT(1, "glk_put_char_stream(NULL) does not crash");

        /* ---- glk_put_string_stream with NULL stream (no crash) ---- */

        glk_put_string_stream(NULL, "test");
        TEST_ASSERT(1, "glk_put_string_stream(NULL) does not crash");

        /* ---- glk_put_buffer_stream with NULL stream (no crash) ---- */

        glk_put_buffer_stream(NULL, "test", 4);
        TEST_ASSERT(1, "glk_put_buffer_stream(NULL) does not crash");

        /* ---- glk_put_string_stream with NULL string (no crash) ---- */

        glk_put_string_stream(str, NULL);
        TEST_ASSERT(1, "glk_put_string_stream(str, NULL) does not crash");

        /* ---- glk_put_buffer_stream with NULL buffer (no crash) ---- */

        glk_put_buffer_stream(str, NULL, 4);
        TEST_ASSERT(1, "glk_put_buffer_stream(str, NULL) does not crash");

        /* ---- glk_put_buffer_stream with zero length (no output) ---- */

        reset_counters();
        glk_put_buffer_stream(str, "test", 0);
        TEST_ASSERT(nextglk_put_char_count == 0,
            "glk_put_buffer_stream with len=0 produces no output");

        /* ---- glk_put_string_stream with empty string (no output) ---- */

        reset_counters();
        glk_put_string_stream(str, "");
        TEST_ASSERT(nextglk_put_char_count == 0,
            "glk_put_string_stream with empty string produces no output");

        /* ---- Window stream is in the stream list ---- */

        found = 0;
        iter = glk_stream_iterate(NULL, NULL);
        while (iter) {
            if (iter == str)
                found = 1;
            iter = glk_stream_iterate(iter, NULL);
        }
        TEST_ASSERT(found == 1,
            "window stream is present in gli_streamlist");

        /* Clean up */
        glk_window_close(win, NULL);
    }

    /* ---- glk_stream_get_current after window close ---- */

    TEST_ASSERT(glk_stream_get_current() == NULL,
        "glk_stream_get_current() returns NULL after window close (current was cleared)");

    /* ---- glk_set_style_stream with NULL stream (no crash) ---- */

    glk_set_style_stream(NULL, 0);
    TEST_ASSERT(1, "glk_set_style_stream(NULL) does not crash");

    return 0;
}