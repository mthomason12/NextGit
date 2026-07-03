/* window_tests.c — Window lifecycle tests for Commit 2
 *
 * Tests:
 *   - glk_window_open() creates window and stream
 *   - window owns its stream (stream type == strtype_Window)
 *   - glk_window_get_root() returns the window
 *   - glk_window_get_stream() returns the window's stream
 *   - glk_window_iterate() iteration (single-window)
 *   - glk_set_window() sets current stream
 *   - glk_window_close() destroys window and owned stream
 *   - gli_mainwin is maintained correctly
 *   - stream list updates correctly on window close
 */

#include "../nextglk/nextglk_internal.h"
#include "../nextglk/nextglk.h"
#include "test_common.h"
#include <string.h>

int window_tests_run(void)
{
    winid_t win;
    strid_t str;
    stream_t *s;
    stream_t *iter;
    stream_result_t res;

    /* ---- Initial state ---- */

    TEST_ASSERT(gli_mainwin == NULL,
        "gli_mainwin is NULL initially");

    win = glk_window_get_root();
    TEST_ASSERT(win == NULL,
        "glk_window_get_root() returns NULL initially");

    /* ---- glk_window_open creates a window ---- */

    win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    TEST_ASSERT(win != NULL,
        "glk_window_open() returns non-NULL");
    TEST_ASSERT(gli_mainwin == (window_t *)win,
        "gli_mainwin == returned window");
    TEST_ASSERT(gli_mainwin->str != NULL,
        "window has a non-NULL stream");

    /* ---- Window owns its stream ---- */

    s = (stream_t *)gli_mainwin->str;
    TEST_ASSERT(s->type == strtype_Window,
        "window stream type == strtype_Window");
    TEST_ASSERT(s->writable == 1,
        "window stream is writable");
    TEST_ASSERT(s->readable == 0,
        "window stream is not readable");
    TEST_ASSERT(s->win == gli_mainwin,
        "window stream back-pointer == window");

    /* ---- glk_window_get_root ---- */

    win = glk_window_get_root();
    TEST_ASSERT(win == gli_mainwin,
        "glk_window_get_root() == gli_mainwin");

    /* ---- glk_window_get_stream ---- */

    str = glk_window_get_stream(gli_mainwin);
    TEST_ASSERT(str == gli_mainwin->str,
        "glk_window_get_stream() returns window's stream");

    /* ---- glk_window_get_type ---- */

    {
        glui32 wtype = glk_window_get_type(gli_mainwin);
        TEST_ASSERT(wtype == wintype_TextBuffer,
            "glk_window_get_type() == wintype_TextBuffer");
    }

    /* ---- glk_window_get_size ---- */

    {
        glui32 width, height;
        glk_window_get_size(gli_mainwin, &width, &height);
        TEST_ASSERT(width == 80,
            "glk_window_get_size() width == 80");
        TEST_ASSERT(height == 25,
            "glk_window_get_size() height == 25");
    }

    /* ---- glk_window_get_sibling ---- */

    win = glk_window_get_sibling(gli_mainwin);
    TEST_ASSERT(win == NULL,
        "glk_window_get_sibling() returns NULL (single window)");

    /* ---- glk_window_get_parent ---- */

    win = glk_window_get_parent(gli_mainwin);
    TEST_ASSERT(win == NULL,
        "glk_window_get_parent() returns NULL (single window)");

    /* ---- glk_window_get_rock ---- */

    {
        glui32 rock = glk_window_get_rock(gli_mainwin);
        TEST_ASSERT(rock == 0,
            "glk_window_get_rock() returns 0 (stub)");
    }

    /* ---- glk_window_iterate (single-window) ---- */

    /* First call with NULL returns gli_mainwin */
    win = glk_window_iterate(NULL, NULL);
    TEST_ASSERT(win == gli_mainwin,
        "glk_window_iterate(NULL) returns gli_mainwin");

    /* Second call with gli_mainwin returns NULL */
    win = glk_window_iterate(gli_mainwin, NULL);
    TEST_ASSERT(win == NULL,
        "glk_window_iterate(mainwin) returns NULL");

    /* Call with invalid window returns NULL */
    win = glk_window_iterate((winid_t)0xFFFFFFFF, NULL);
    TEST_ASSERT(win == NULL,
        "glk_window_iterate(invalid) returns NULL");

    /* With rockptr */
    {
        glui32 rock;
        win = glk_window_iterate(NULL, &rock);
        TEST_ASSERT(win == gli_mainwin,
            "glk_window_iterate(NULL, &rock) returns gli_mainwin");
        TEST_ASSERT(rock == 0,
            "rock value is 0");
    }

    /* ---- glk_set_window sets current stream ---- */

    TEST_ASSERT(gli_currentstr != gli_mainwin->str,
        "current stream is NOT yet the window stream (may be NULL)");
    glk_set_window(gli_mainwin);
    TEST_ASSERT(gli_currentstr == gli_mainwin->str,
        "glk_set_window() sets gli_currentstr to window stream");

    /* ---- Stream list contains the window stream ---- */

    {
        int found = 0;
        iter = glk_stream_iterate(NULL, NULL);
        while (iter) {
            if (iter == gli_mainwin->str)
                found = 1;
            iter = glk_stream_iterate(iter, NULL);
        }
        TEST_ASSERT(found == 1,
            "window stream is present in gli_streamlist");
    }

    /* ---- glk_window_close destroys window ---- */

    memset(&res, 0, sizeof(res));
    glk_window_close(gli_mainwin, &res);
    TEST_ASSERT(gli_mainwin == NULL,
        "gli_mainwin == NULL after window close");
    TEST_ASSERT(gli_currentstr == NULL,
        "gli_currentstr == NULL after window close (owned stream was deleted)");

    /* ---- Window stream removed from stream list ---- */

    {
        int found = 0;
        iter = glk_stream_iterate(NULL, NULL);
        while (iter) {
            if (iter == (stream_t *)str)
                found = 1;
            iter = glk_stream_iterate(iter, NULL);
        }
        TEST_ASSERT(found == 0,
            "window stream is removed from gli_streamlist after close");
    }

    /* ---- Second call to glk_window_open after close succeeds ---- */

    win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    TEST_ASSERT(win != NULL,
        "glk_window_open() succeeds after previous window was closed");
    TEST_ASSERT(gli_mainwin == (window_t *)win,
        "gli_mainwin is the new window");

    /* Clean up */
    glk_window_close(gli_mainwin, NULL);

    /* ---- glk_window_open with wrong wintype returns NULL ---- */

    win = glk_window_open(NULL, 0, 0, wintype_TextGrid, 0);
    TEST_ASSERT(win == NULL,
        "glk_window_open() with wintype_TextGrid returns NULL");
    TEST_ASSERT(gli_mainwin == NULL,
        "gli_mainwin is still NULL after rejected open");

    /* ---- glk_window_open when window already exists returns NULL ---- */

    win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    TEST_ASSERT(win != NULL,
        "first window opens successfully");

    {
        winid_t win2 = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
        TEST_ASSERT(win2 == NULL,
            "second glk_window_open() returns NULL (single-window constraint)");
        TEST_ASSERT(gli_mainwin == (window_t *)win,
            "gli_mainwin unchanged after failed second open");
    }

    /* Clean up */
    glk_window_close(gli_mainwin, NULL);

    /* ---- glk_window_close with NULL/no-op (no crash) ---- */

    glk_window_close(NULL, NULL);
    TEST_ASSERT(1, "glk_window_close(NULL) does not crash");

    glk_window_close((winid_t)0xDEADBEEF, NULL);
    TEST_ASSERT(1, "glk_window_close(invalid window) does not crash");

    /* ---- glk_window_clear (no crash) ---- */

    win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    TEST_ASSERT(win != NULL, "opened window for clear test");
    glk_window_clear(gli_mainwin);
    TEST_ASSERT(1, "glk_window_clear() does not crash");
    glk_window_close(gli_mainwin, NULL);

    /* ---- glk_window_move_cursor (no crash) ---- */

    win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    TEST_ASSERT(win != NULL, "opened window for move_cursor test");
    glk_window_move_cursor(gli_mainwin, 5, 10);
    TEST_ASSERT(1, "glk_window_move_cursor() does not crash");
    glk_window_close(gli_mainwin, NULL);

    return 0;
}