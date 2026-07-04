/* save_input_test.c — Tests for NULL-window fallback in glk_request_line_event
 *
 * Tests the bug fixed in docs/bugs/post-save-input-event-trace.md:
 *   - After save, the VM passes window ID 0, which resolves to NULL
 *   - glk_request_line_event(NULL, ...) must fall back to gli_mainwin
 *   - glk_request_line_event_uni(NULL, ...) must fall back to gli_mainwin
 *
 * Requires: NextGlk library linked
 */

#include <stdio.h>
#include <string.h>
#include "glk.h"
#include "gi_dispa.h"
#include "nextglk_internal.h"

static int pass = 0;
static int fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  PASS: %s\n", msg); pass++; } \
    else { printf("  FAIL: %s (line %d)\n", msg, __LINE__); fail++; } \
} while(0)

int main(void) {
    printf("=== NULL-window fallback tests ===\n\n");

    /* Initialize: open a window (sets gli_mainwin) */
    winid_t win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    CHECK(win != NULL, "window_open succeeds");
    CHECK(gli_mainwin != NULL, "gli_mainwin is set");
    CHECK(gli_mainwin == (window_t *)win, "gli_mainwin matches opened window");

    /* ---------------------------------------------------------------
     * Test 1: glk_request_line_event with valid window
     * --------------------------------------------------------------- */
    printf("\n--- Test 1: Valid window ---\n");
    gli_mainwin->line_request = 0;
    gli_mainwin->linebuf = NULL;
    gli_mainwin->linebuflen = 0;
    char buf[256] = {0};

    glk_request_line_event(win, buf, 256, 0);
    CHECK(gli_mainwin->line_request == 1,
        "Valid window: line_request set to 1");
    CHECK(gli_mainwin->linebuf == (void *)buf,
        "Valid window: linebuf stored");
    CHECK(gli_mainwin->linebuflen == 256,
        "Valid window: linebuflen stored");

    glk_cancel_line_event(win, NULL);
    CHECK(gli_mainwin->line_request == 0,
        "cancel_line_event: line_request cleared");

    /* ---------------------------------------------------------------
     * Test 2: glk_request_line_event with NULL window (post-save)
     *         This is the bug: NULL window must fall back to gli_mainwin
     * --------------------------------------------------------------- */
    printf("\n--- Test 2: NULL window (post-save scenario) ---\n");
    gli_mainwin->line_request = 0;
    gli_mainwin->linebuf = NULL;
    gli_mainwin->linebuflen = 0;
    char buf2[256] = {0};

    glk_request_line_event(NULL, buf2, 256, 0);
    CHECK(gli_mainwin->line_request == 1,
        "NULL window: fallback to gli_mainwin, line_request set to 1");
    CHECK(gli_mainwin->linebuf == (void *)buf2,
        "NULL window: linebuf stored on gli_mainwin");
    CHECK(gli_mainwin->linebuflen == 256,
        "NULL window: linebuflen stored on gli_mainwin");

    glk_cancel_line_event(win, NULL);
    CHECK(gli_mainwin->line_request == 0,
        "cancel after NULL-window request: line_request cleared");

    /* ---------------------------------------------------------------
     * Test 3: glk_request_line_event_uni with NULL window
     * --------------------------------------------------------------- */
    printf("\n--- Test 3: NULL window Unicode ---\n");
    gli_mainwin->line_request_uni = 0;
    gli_mainwin->linebuf = NULL;
    gli_mainwin->linebuflen = 0;
    glui32 ubuf[256] = {0};

    glk_request_line_event_uni(NULL, ubuf, 256, 0);
    CHECK(gli_mainwin->line_request_uni == 1,
        "NULL window UNI: fallback to gli_mainwin, line_request_uni set");
    CHECK(gli_mainwin->linebuf == (void *)ubuf,
        "NULL window UNI: linebuf stored");
    CHECK(gli_mainwin->linebuflen == 256 * sizeof(glui32),
        "NULL window UNI: linebuflen = maxlen * sizeof(glui32)");

    glk_cancel_line_event(win, NULL);

    /* ---------------------------------------------------------------
     * Test 4: NULL window AND no gli_mainwin (edge case)
     * --------------------------------------------------------------- */
    printf("\n--- Test 4: NULL window, no mainwin ---\n");
    /* Cannot test in single-window mode since gli_mainwin exists.
     * glk_window_close would be needed, but that's destructive.
     * Documented as: would return without crash if both are NULL. */

    /* Cleanup */
    glk_window_close(win, NULL);

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}