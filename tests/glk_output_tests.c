/* glk_output_tests.c — Commit 5 tests: convenience output, gestalt, event loop, stubs
 *
 * Tests:
 *   - glk_put_char() routes through current stream to nextglk_put_char()
 *   - glk_put_string() routes through current stream to nextglk_put_char()
 *   - glk_put_buffer() routes through current stream to nextglk_put_char()
 *   - glk_set_style() is safe to call with current stream
 *   - No output when gli_currentstr is NULL
 *   - glk_char_to_lower() ASCII conversion
 *   - glk_char_to_upper() ASCII conversion
 *   - glk_gestalt() returns sensible Phase 1 values
 *   - glk_gestalt_ext() returns sensible Phase 1 values
 *   - glk_select() populates event with evtype_None
 *   - glk_select_poll() populates event with evtype_None
 *   - All Commit 5 stubs are callable and safe
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

int glk_output_tests_run(void)
{
    winid_t win;
    strid_t str;

    /* ---- glk_gestalt returns sensible Phase 1 values ---- */

    {
        glui32 result;

        result = glk_gestalt(gestalt_Version, 0);
        TEST_ASSERT(result == 0x00070600,
            "glk_gestalt(gestalt_Version) returns 0x00070600");

        result = glk_gestalt(gestalt_CharOutput, 0);
        TEST_ASSERT(result == gestalt_CharOutput_ExactPrint,
            "glk_gestalt(gestalt_CharOutput) returns gestalt_CharOutput_ExactPrint");

        result = glk_gestalt(gestalt_Unicode, 0);
        TEST_ASSERT(result == 1,
            "glk_gestalt(gestalt_Unicode) returns 1");

        result = glk_gestalt(gestalt_MouseInput, 0);
        TEST_ASSERT(result == 0,
            "glk_gestalt(gestalt_MouseInput) returns 0 (unsupported)");

        result = glk_gestalt(gestalt_Timer, 0);
        TEST_ASSERT(result == 0,
            "glk_gestalt(gestalt_Timer) returns 0 (unsupported)");

        result = glk_gestalt(gestalt_Graphics, 0);
        TEST_ASSERT(result == 0,
            "glk_gestalt(gestalt_Graphics) returns 0 (unsupported)");

        result = glk_gestalt(gestalt_Sound, 0);
        TEST_ASSERT(result == 0,
            "glk_gestalt(gestalt_Sound) returns 0 (unsupported)");

        result = glk_gestalt(gestalt_Hyperlinks, 0);
        TEST_ASSERT(result == 0,
            "glk_gestalt(gestalt_Hyperlinks) returns 0 (unsupported)");

        result = glk_gestalt(gestalt_DateTime, 0);
        TEST_ASSERT(result == 0,
            "glk_gestalt(gestalt_DateTime) returns 0 (unsupported)");
    }

    /* ---- glk_gestalt_ext returns sensible Phase 1 values ---- */

    {
        glui32 result;
        glui32 arr[4];

        result = glk_gestalt_ext(gestalt_Version, 0, NULL, 0);
        TEST_ASSERT(result == 0x00070600,
            "glk_gestalt_ext(gestalt_Version) returns 0x00070600");

        result = glk_gestalt_ext(gestalt_CharOutput, 0, NULL, 0);
        TEST_ASSERT(result == gestalt_CharOutput_ExactPrint,
            "glk_gestalt_ext(gestalt_CharOutput) returns gestalt_CharOutput_ExactPrint");

        /* Extended query with array — should fill array with zeroes */
        arr[0] = 0xFFFFFFFF;
        arr[1] = 0xFFFFFFFF;
        result = glk_gestalt_ext(gestalt_Version, 0, arr, 2);
        TEST_ASSERT(result == 0x00070600,
            "glk_gestalt_ext with arr returns correct value");
        TEST_ASSERT(arr[0] == 0,
            "glk_gestalt_ext fills arr[0] with 0");
        TEST_ASSERT(arr[1] == 0,
            "glk_gestalt_ext fills arr[1] with 0");
    }

    /* ---- glk_char_to_lower ---- */

    {
        TEST_ASSERT(glk_char_to_lower('A') == 'a',
            "glk_char_to_lower('A') returns 'a'");
        TEST_ASSERT(glk_char_to_lower('Z') == 'z',
            "glk_char_to_lower('Z') returns 'z'");
        TEST_ASSERT(glk_char_to_lower('a') == 'a',
            "glk_char_to_lower('a') returns 'a' (already lowercase)");
        TEST_ASSERT(glk_char_to_lower('!') == '!',
            "glk_char_to_lower('!') returns '!' (non-alpha unchanged)");
        TEST_ASSERT(glk_char_to_lower('0') == '0',
            "glk_char_to_lower('0') returns '0' (digit unchanged)");
        TEST_ASSERT(glk_char_to_lower('M') == 'm',
            "glk_char_to_lower('M') returns 'm'");
    }

    /* ---- glk_char_to_upper ---- */

    {
        TEST_ASSERT(glk_char_to_upper('a') == 'A',
            "glk_char_to_upper('a') returns 'A'");
        TEST_ASSERT(glk_char_to_upper('z') == 'Z',
            "glk_char_to_upper('z') returns 'Z'");
        TEST_ASSERT(glk_char_to_upper('A') == 'A',
            "glk_char_to_upper('A') returns 'A' (already uppercase)");
        TEST_ASSERT(glk_char_to_upper('!') == '!',
            "glk_char_to_upper('!') returns '!' (non-alpha unchanged)");
        TEST_ASSERT(glk_char_to_upper('0') == '0',
            "glk_char_to_upper('0') returns '0' (digit unchanged)");
        TEST_ASSERT(glk_char_to_upper('m') == 'M',
            "glk_char_to_upper('m') returns 'M'");
    }

    /* ---- glk_select and glk_select_poll ---- */

    {
        event_t ev;
        ev.type = 999;
        ev.win = (winid_t)0xDEADBEEF;
        ev.val1 = 999;
        ev.val2 = 999;

        glk_select(&ev);
        TEST_ASSERT(ev.type == evtype_None,
            "glk_select() sets event type to evtype_None");
        TEST_ASSERT(ev.win == NULL,
            "glk_select() sets event win to NULL");
        TEST_ASSERT(ev.val1 == 0,
            "glk_select() sets event val1 to 0");
        TEST_ASSERT(ev.val2 == 0,
            "glk_select() sets event val2 to 0");

        ev.type = 999;
        ev.win = (winid_t)0xDEADBEEF;
        ev.val1 = 999;
        ev.val2 = 999;

        glk_select_poll(&ev);
        TEST_ASSERT(ev.type == evtype_None,
            "glk_select_poll() sets event type to evtype_None");
        TEST_ASSERT(ev.win == NULL,
            "glk_select_poll() sets event win to NULL");
        TEST_ASSERT(ev.val1 == 0,
            "glk_select_poll() sets event val1 to 0");
        TEST_ASSERT(ev.val2 == 0,
            "glk_select_poll() sets event val2 to 0");
    }

    /* ---- glk_select with NULL event (no crash) ---- */

    {
        glk_select(NULL);
        TEST_ASSERT(1, "glk_select(NULL) does not crash");

        glk_select_poll(NULL);
        TEST_ASSERT(1, "glk_select_poll(NULL) does not crash");
    }

    /* ---- Convenience output: glk_put_char, glk_put_string, glk_put_buffer
     *
     * These require a window with a current stream set.
     * ---- */

    {
        stream_t *s;

        /* Open a window */
        win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
        TEST_ASSERT(win != NULL, "glk_window_open() succeeds for convenience output test");

        /* Get the window's stream */
        str = glk_window_get_stream(win);
        TEST_ASSERT(str != NULL, "window stream is non-NULL");
        s = (stream_t *)str;

        /* ---- glk_put_char with no current stream (no output) ---- */

        reset_counters();
        glk_put_char('A');
        TEST_ASSERT(nextglk_put_char_count == 0,
            "glk_put_char('A') with no current stream produces no output");

        /* ---- Set current stream via glk_set_window ---- */

        glk_set_window(win);
        TEST_ASSERT(glk_stream_get_current() == str,
            "glk_stream_get_current() returns window stream after glk_set_window()");

        /* ---- glk_put_char routes through current stream ---- */

        reset_counters();
        s->writecount = 0;

        glk_put_char('A');
        TEST_ASSERT(nextglk_put_char_count == 1,
            "glk_put_char('A') with current stream calls nextglk_put_char 1 time");
        TEST_ASSERT(s->writecount == 1,
            "stream writecount incremented to 1 after glk_put_char('A')");

        glk_put_char('B');
        TEST_ASSERT(nextglk_put_char_count == 2,
            "glk_put_char('B') with current stream calls nextglk_put_char 2 times");
        TEST_ASSERT(s->writecount == 2,
            "stream writecount incremented to 2 after glk_put_char('B')");

        /* ---- glk_put_string routes through current stream ---- */

        reset_counters();
        s->writecount = 0;

        glk_put_string("Hello");
        TEST_ASSERT(nextglk_put_char_count == 5,
            "glk_put_string(\"Hello\") with current stream calls nextglk_put_char 5 times");
        TEST_ASSERT(s->writecount == 5,
            "stream writecount incremented to 5 after glk_put_string(\"Hello\")");

        /* ---- glk_put_buffer routes through current stream ---- */

        reset_counters();
        s->writecount = 0;

        glk_put_buffer("AB", 2);
        TEST_ASSERT(nextglk_put_char_count == 2,
            "glk_put_buffer(\"AB\", 2) with current stream calls nextglk_put_char 2 times");
        TEST_ASSERT(s->writecount == 2,
            "stream writecount incremented to 2 after glk_put_buffer(\"AB\", 2)");

        /* ---- glk_put_buffer with longer buffer ---- */

        reset_counters();
        s->writecount = 0;

        glk_put_buffer("HelloWorld", 10);
        TEST_ASSERT(nextglk_put_char_count == 10,
            "glk_put_buffer(\"HelloWorld\", 10) calls nextglk_put_char 10 times");
        TEST_ASSERT(s->writecount == 10,
            "stream writecount incremented to 10 after glk_put_buffer(\"HelloWorld\", 10)");

        /* ---- glk_set_style with current stream (no crash) ---- */

        glk_set_style(style_Normal);
        TEST_ASSERT(1, "glk_set_style(style_Normal) with current stream does not crash");

        glk_set_style(style_Emphasized);
        TEST_ASSERT(1, "glk_set_style(style_Emphasized) with current stream does not crash");

        glk_set_style(999);
        TEST_ASSERT(1, "glk_set_style(999) with current stream does not crash");

        /* ---- glk_put_char with NULL string (no crash) ---- */

        glk_put_string(NULL);
        TEST_ASSERT(1, "glk_put_string(NULL) with current stream does not crash");

        /* ---- glk_put_buffer with NULL buffer (no crash) ---- */

        glk_put_buffer(NULL, 4);
        TEST_ASSERT(1, "glk_put_buffer(NULL, 4) with current stream does not crash");

        /* ---- glk_put_buffer with zero length (no output) ---- */

        reset_counters();
        glk_put_buffer("test", 0);
        TEST_ASSERT(nextglk_put_char_count == 0,
            "glk_put_buffer with len=0 produces no output");

        /* ---- glk_put_string with empty string (no output) ---- */

        reset_counters();
        glk_put_string("");
        TEST_ASSERT(nextglk_put_char_count == 0,
            "glk_put_string with empty string produces no output");

        /* ---- glk_set_style with no current stream (no crash) ---- */

        glk_stream_set_current(NULL);
        glk_set_style(style_Normal);
        TEST_ASSERT(1, "glk_set_style(style_Normal) with no current stream does not crash");

        /* Clean up */
        glk_window_close(win, NULL);
    }

    /* ---- All Commit 5 stubs are callable and safe ---- */

    {
        /* glk_tick */
        glk_tick();
        TEST_ASSERT(1, "glk_tick() does not crash");

        /* glk_set_interrupt_handler */
        glk_set_interrupt_handler(NULL);
        TEST_ASSERT(1, "glk_set_interrupt_handler(NULL) does not crash");

        /* glk_window_get_rock */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glui32 rock = glk_window_get_rock(w);
                TEST_ASSERT(rock == 0,
                    "glk_window_get_rock() returns 0");
                glk_window_close(w, NULL);
            }
        }

        /* glk_window_get_parent */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                winid_t parent = glk_window_get_parent(w);
                TEST_ASSERT(parent == NULL,
                    "glk_window_get_parent() returns NULL");
                glk_window_close(w, NULL);
            }
        }

        /* glk_window_set_arrangement */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_window_set_arrangement(w, 0, 0, NULL);
                TEST_ASSERT(1, "glk_window_set_arrangement() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_window_get_arrangement */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glui32 method, size;
                winid_t keywin;
                glk_window_get_arrangement(w, &method, &size, &keywin);
                TEST_ASSERT(method == 0,
                    "glk_window_get_arrangement() sets method to 0");
                TEST_ASSERT(size == 0,
                    "glk_window_get_arrangement() sets size to 0");
                TEST_ASSERT(keywin == NULL,
                    "glk_window_get_arrangement() sets keywin to NULL");
                glk_window_close(w, NULL);
            }
        }

        /* glk_stream_get_rock */
        {
            stream_t *s = gli_new_stream(strtype_Memory, 0, 1, 0);
            if (s) {
                glui32 rock = glk_stream_get_rock((strid_t)s);
                TEST_ASSERT(rock == 0,
                    "glk_stream_get_rock() returns 0");
                gli_delete_stream(s, NULL);
            }
        }

        /* glk_stream_open_memory */
        {
            strid_t ms = glk_stream_open_memory(NULL, 0, 0, 0);
            TEST_ASSERT(ms == NULL,
                "glk_stream_open_memory() returns NULL (stub)");
        }

        /* glk_stream_close with non-window stream */
        {
            stream_t *s = gli_new_stream(strtype_Memory, 0, 1, 0);
            if (s) {
                glk_stream_close((strid_t)s, NULL);
                TEST_ASSERT(1, "glk_stream_close() on memory stream does not crash");
            }
        }

        /* glk_stream_close with window stream (should be rejected) */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                strid_t ws = glk_window_get_stream(w);
                /* This should be a no-op (window streams are owned by window) */
                glk_stream_close(ws, NULL);
                TEST_ASSERT(1, "glk_stream_close() on window stream does not crash (no-op)");
                glk_window_close(w, NULL);
            }
        }

        /* glk_stream_set_position */
        {
            stream_t *s = gli_new_stream(strtype_Memory, 0, 1, 0);
            if (s) {
                glk_stream_set_position((strid_t)s, 0, seekmode_Start);
                TEST_ASSERT(1, "glk_stream_set_position() does not crash");
                gli_delete_stream(s, NULL);
            }
        }

        /* glk_stream_get_position */
        {
            stream_t *s = gli_new_stream(strtype_Memory, 0, 1, 0);
            if (s) {
                glui32 pos = glk_stream_get_position((strid_t)s);
                TEST_ASSERT(pos == 0,
                    "glk_stream_get_position() returns 0");
                gli_delete_stream(s, NULL);
            }
        }

        /* glk_get_char_stream */
        {
            stream_t *s = gli_new_stream(strtype_Memory, 1, 0, 0);
            if (s) {
                glsi32 ch = glk_get_char_stream((strid_t)s);
                TEST_ASSERT(ch == -1,
                    "glk_get_char_stream() returns -1 (stub)");
                gli_delete_stream(s, NULL);
            }
        }

        /* glk_get_line_stream */
        {
            stream_t *s = gli_new_stream(strtype_Memory, 1, 0, 0);
            if (s) {
                char buf[10];
                glui32 n = glk_get_line_stream((strid_t)s, buf, 10);
                TEST_ASSERT(n == 0,
                    "glk_get_line_stream() returns 0 (stub)");
                gli_delete_stream(s, NULL);
            }
        }

        /* glk_get_buffer_stream */
        {
            stream_t *s = gli_new_stream(strtype_Memory, 1, 0, 0);
            if (s) {
                char buf[10];
                glui32 n = glk_get_buffer_stream((strid_t)s, buf, 10);
                TEST_ASSERT(n == 0,
                    "glk_get_buffer_stream() returns 0 (stub)");
                gli_delete_stream(s, NULL);
            }
        }

        /* glk_stream_open_file */
        {
            strid_t fs = glk_stream_open_file(NULL, 0, 0);
            TEST_ASSERT(fs == NULL,
                "glk_stream_open_file() returns NULL (stub)");
        }

        /* glk_style_distinguish */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glui32 d = glk_style_distinguish(w, style_Normal, style_Emphasized);
                TEST_ASSERT(d == 1,
                    "glk_style_distinguish() returns 1 (indistinguishable)");
                glk_window_close(w, NULL);
            }
        }

        /* glk_style_measure */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glui32 result;
                glui32 avail = glk_style_measure(w, style_Normal, stylehint_Size, &result);
                TEST_ASSERT(avail == 0,
                    "glk_style_measure() returns 0 (not available)");
                TEST_ASSERT(result == 0,
                    "glk_style_measure() sets result to 0");
                glk_window_close(w, NULL);
            }
        }

        /* glk_request_line_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_request_line_event(w, NULL, 0, 0);
                TEST_ASSERT(1, "glk_request_line_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_cancel_line_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_cancel_line_event(w, NULL);
                TEST_ASSERT(1, "glk_cancel_line_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_request_char_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_request_char_event(w);
                TEST_ASSERT(1, "glk_request_char_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_cancel_char_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_cancel_char_event(w);
                TEST_ASSERT(1, "glk_cancel_char_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_request_mouse_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_request_mouse_event(w);
                TEST_ASSERT(1, "glk_request_mouse_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_cancel_mouse_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_cancel_mouse_event(w);
                TEST_ASSERT(1, "glk_cancel_mouse_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_request_timer_events */
        {
            glk_request_timer_events(1000);
            TEST_ASSERT(1, "glk_request_timer_events(1000) does not crash");
        }

        /* glk_window_set_echo_stream */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_window_set_echo_stream(w, NULL);
                TEST_ASSERT(1, "glk_window_set_echo_stream() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_window_get_echo_stream */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                strid_t es = glk_window_get_echo_stream(w);
                TEST_ASSERT(es == NULL,
                    "glk_window_get_echo_stream() returns NULL");
                glk_window_close(w, NULL);
            }
        }

        /* glk_set_echo_line_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glk_set_echo_line_event(w, 1);
                TEST_ASSERT(1, "glk_set_echo_line_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_set_terminators_line_event */
        {
            winid_t w = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
            if (w) {
                glui32 keycodes[] = { keycode_Escape, keycode_Tab };
                glk_set_terminators_line_event(w, keycodes, 2);
                TEST_ASSERT(1, "glk_set_terminators_line_event() does not crash");
                glk_window_close(w, NULL);
            }
        }

        /* glk_stylehint_set */
        {
            glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_Size, 12);
            TEST_ASSERT(1, "glk_stylehint_set() does not crash");
        }

        /* glk_stylehint_clear */
        {
            glk_stylehint_clear(wintype_TextBuffer, style_Normal, stylehint_Size);
            TEST_ASSERT(1, "glk_stylehint_clear() does not crash");
        }
    }

    return 0;
}