/* input_tests.c — Tests for Glk line-input system (Runtime Attempt 2)
 *
 * Tests:
 *   - Line request registration (buffer pointer, capacity, flag)
 *   - Line request cancellation
 *   - Cancel with event pointer (populated correctly)
 *   - glk_select() with no pending request (returns evtype_None)
 *   - glk_select() with line request pending (reads from stdin)
 *   - Line length reporting in event
 *
 * Note: Tests that require stdin interaction use a temporary pipe to
 * simulate user input.
 */

#include "test_common.h"
#include "nextglk_internal.h"
#include "nextglk.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Test suite entry point */
int input_tests_run(void);

/* -------------------------------------------------------------------------
 * Test helpers
 * ------------------------------------------------------------------------- */

/* Forward declarations for internal functions not in headers */
extern stream_t *gli_new_stream(int type, int readable, int writable, glui32 rock);
extern window_t *gli_new_window(glui32 rock);

/* Create a test window with a stream, suitable for input tests */
static window_t *create_test_window(void)
{
    window_t *win = gli_new_window(42);
    if (!win)
        return NULL;

    stream_t *str = gli_new_stream(strtype_Window, 0, 1, 0);
    if (!str) {
        gli_delete_window(win, NULL);
        return NULL;
    }

    win->str = str;
    str->win = win;

    /* Set as main window for glk_select() */
    gli_mainwin = win;

    return win;
}

/* Destroy a test window and its stream */
static void destroy_test_window(window_t *win)
{
    if (!win)
        return;

    if (win->str) {
        win->str->win = NULL;
        gli_delete_stream(win->str, NULL);
        win->str = NULL;
    }

    gli_mainwin = NULL;
    gli_delete_window(win, NULL);
}

/* Redirect stdin to a pipe containing the given string */
static int pipe_input_to_stdin(const char *input)
{
    int pd[2];

    if (pipe(pd) != 0)
        return -1;

    /* Write input to pipe */
    write(pd[1], input, strlen(input));
    close(pd[1]);

    /* Redirect stdin to read end */
    if (dup2(pd[0], STDIN_FILENO) < 0) {
        close(pd[0]);
        return -1;
    }

    /* Close original read end (now duplicated) */
    close(pd[0]);

    return 0;
}

/* -------------------------------------------------------------------------
 * Test: Line request registration
 * ------------------------------------------------------------------------- */

static int test_line_request_registration(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_line_request_registration (setup failed)\n");
        return 1;
    }

    char buf[256];

    /* Verify initial state */
    TEST_ASSERT(win->line_request == 0, "line_request starts as 0");
    TEST_ASSERT(win->linebuf == NULL, "linebuf starts as NULL");
    TEST_ASSERT(win->linebuflen == 0, "linebuflen starts as 0");

    /* Request line input */
    glk_request_line_event((winid_t)win, buf, sizeof(buf), 0);

    /* Verify state after request */
    TEST_ASSERT(win->line_request == 1, "line_request set to 1 after request");
    TEST_ASSERT(win->linebuf == (void *)buf, "linebuf stores buffer pointer");
    TEST_ASSERT(win->linebuflen == sizeof(buf), "linebuflen stores capacity");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: Line request cancellation (no event pointer)
 * ------------------------------------------------------------------------- */

static int test_line_request_cancellation(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_line_request_cancellation (setup failed)\n");
        return 1;
    }

    char buf[256];

    /* Request and verify */
    glk_request_line_event((winid_t)win, buf, sizeof(buf), 0);
    TEST_ASSERT(win->line_request == 1, "line_request set after request");

    /* Cancel without event pointer */
    glk_cancel_line_event((winid_t)win, NULL);
    TEST_ASSERT(win->line_request == 0, "line_request cleared after cancel");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: Cancel with event pointer
 * ------------------------------------------------------------------------- */

static int test_cancel_with_event(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_cancel_with_event (setup failed)\n");
        return 1;
    }

    event_t ev;
    char buf[256];

    /* Request line input */
    glk_request_line_event((winid_t)win, buf, 256, 0);

    /* Cancel with event pointer */
    glk_cancel_line_event((winid_t)win, &ev);

    /* Verify event populated correctly */
    TEST_ASSERT(ev.type == evtype_LineInput, "event type is evtype_LineInput");
    TEST_ASSERT(ev.win == (winid_t)win, "event win matches");
    TEST_ASSERT(ev.val1 == 0, "event val1 is 0 (cancelled)");
    TEST_ASSERT(ev.val2 == 0, "event val2 is 0");
    TEST_ASSERT(win->line_request == 0, "line_request cleared");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: glk_select() with no pending request
 * ------------------------------------------------------------------------- */

static int test_select_no_request(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_select_no_request (setup failed)\n");
        return 1;
    }

    event_t ev;

    /* No pending request on main window */
    win->line_request = 0;

    glk_select(&ev);

    TEST_ASSERT(ev.type == evtype_None, "event type is evtype_None");
    TEST_ASSERT(ev.win == NULL, "event win is NULL");
    TEST_ASSERT(ev.val1 == 0, "event val1 is 0");
    TEST_ASSERT(ev.val2 == 0, "event val2 is 0");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: glk_select() with line request — generates event
 * ------------------------------------------------------------------------- */

static int test_select_line_input(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_select_line_input (setup failed)\n");
        return 1;
    }

    event_t ev;
    char buf[256];
    const char *test_input = "hello\n";

    /* Request line input */
    glk_request_line_event((winid_t)win, buf, sizeof(buf), 0);

    /* Pipe test input to stdin */
    if (pipe_input_to_stdin(test_input) != 0) {
        printf("  FAIL: test_select_line_input (pipe setup failed)\n");
        destroy_test_window(win);
        return 1;
    }

    /* Select should read from stdin */
    glk_select(&ev);

    TEST_ASSERT(ev.type == evtype_LineInput, "event type is evtype_LineInput");
    TEST_ASSERT(ev.win == (winid_t)win, "event win matches requesting window");
    TEST_ASSERT(ev.val1 == 5, "event val1 is 5 (length of 'hello')");
    TEST_ASSERT(ev.val2 == 0, "event val2 is 0");
    TEST_ASSERT(strcmp(buf, "hello") == 0, "buffer contains 'hello'");
    TEST_ASSERT(win->line_request == 0, "line_request cleared after select");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: Line length reporting — empty line
 * ------------------------------------------------------------------------- */

static int test_empty_line(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_empty_line (setup failed)\n");
        return 1;
    }

    event_t ev;
    char buf[256];

    /* Request line input */
    glk_request_line_event((winid_t)win, buf, sizeof(buf), 0);

    /* Pipe empty line to stdin */
    if (pipe_input_to_stdin("\n") != 0) {
        printf("  FAIL: test_empty_line (pipe setup failed)\n");
        destroy_test_window(win);
        return 1;
    }

    glk_select(&ev);

    TEST_ASSERT(ev.type == evtype_LineInput, "event type is evtype_LineInput");
    TEST_ASSERT(ev.val1 == 0, "event val1 is 0 for empty line");
    TEST_ASSERT(buf[0] == '\0', "buffer is empty string");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: Line length reporting — max-length input
 * ------------------------------------------------------------------------- */

static int test_line_length_reporting(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_line_length_reporting (setup failed)\n");
        return 1;
    }

    event_t ev;
    char buf[10];  /* Small buffer */

    /* Request line input with small buffer */
    glk_request_line_event((winid_t)win, buf, sizeof(buf), 0);

    /* Pipe longer input to stdin */
    if (pipe_input_to_stdin("abcdefghijklmno\n") != 0) {
        printf("  FAIL: test_line_length_reporting (pipe setup failed)\n");
        destroy_test_window(win);
        return 1;
    }

    glk_select(&ev);

    /* Buffer size is 10, so at most 9 chars + null terminator.
     * fgets reads up to 9 chars. The input "abcdefghijklmno" is 15 chars.
     * fgets will read only 9 chars ("abcdefghi") and will NOT see \n.
     * The reported length should be 9 (the number of chars read). */
    TEST_ASSERT(ev.type == evtype_LineInput, "event type is evtype_LineInput");
    TEST_ASSERT(ev.val1 == 9, "event val1 is 9 (truncated to buffer size)");

    /* Verify no buffer overflow */
    TEST_ASSERT(buf[9] == '\0', "buffer null-terminated within bounds");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: glk_select() with NULL event pointer (should not crash)
 * ------------------------------------------------------------------------- */

static int test_select_null_event(void)
{
    window_t *win = create_test_window();
    if (!win) {
        printf("  FAIL: test_select_null_event (setup failed)\n");
        return 1;
    }

    /* Calling glk_select with NULL event should not crash */
    glk_select(NULL);

    TEST_ASSERT(1, "glk_select(NULL) handled gracefully");

    destroy_test_window(win);
    return 0;
}

/* -------------------------------------------------------------------------
 * Test: Cancel on NULL window (should not crash)
 * ------------------------------------------------------------------------- */

static int test_cancel_null_window(void)
{
    /* Calling glk_cancel_line_event with NULL window should not crash */
    glk_cancel_line_event(NULL, NULL);

    TEST_ASSERT(1, "glk_cancel_line_event(NULL, NULL) handled gracefully");

    return 0;
}

/* -------------------------------------------------------------------------
 * Test: Request on NULL window (should not crash)
 * ------------------------------------------------------------------------- */

static int test_request_null_window(void)
{
    /* Calling glk_request_line_event with NULL window should not crash */
    glk_request_line_event(NULL, NULL, 0, 0);

    TEST_ASSERT(1, "glk_request_line_event(NULL, ...) handled gracefully");

    return 0;
}

/* -------------------------------------------------------------------------
 * Run all input tests
 * ------------------------------------------------------------------------- */

int input_tests_run(void)
{
    int failures = 0;

    printf("  --- Line Request Registration ---\n");
    failures += test_line_request_registration();

    printf("  --- Line Request Cancellation ---\n");
    failures += test_line_request_cancellation();
    failures += test_cancel_with_event();

    printf("  --- Event Generation ---\n");
    failures += test_select_no_request();
    failures += test_select_line_input();

    printf("  --- Line Length Reporting ---\n");
    failures += test_empty_line();
    failures += test_line_length_reporting();

    printf("  --- Edge Cases ---\n");
    failures += test_select_null_event();
    failures += test_cancel_null_window();
    failures += test_request_null_window();

    return failures;
}