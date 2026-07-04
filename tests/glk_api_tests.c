/*
 * glk_api_tests.c — Verify Glk API behaviour against recorded CheapGlk trace.
 *
 * Compiled and linked against a specific Glk library (CheapGlk or NextGlk).
 * Run: ./glk_api_test
 *
 * Tests replicate the key API calls from the CheapGlk save/restore trace
 * and assert expected behaviour.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "glk.h"
#include "gi_dispa.h"

static int passed = 0;
static int failed = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

#define ASSERT_NONNULL(val, msg) do { \
    if ((val) == NULL) { \
        fprintf(stderr, "FAIL: %s (got NULL)\n", msg); \
        failed++; \
    } else { \
        passed++; \
    } \
} while(0)

int main(void) {
    winid_t root, iter;
    frefid_t fref;
    strid_t str;
    glui32 rock;

    fprintf(stderr, "=== Glk API Tests ===\n\n");

    /* --- window_get_root --- */
    root = glk_window_get_root();
    fprintf(stderr, "glk_window_get_root() = %p\n", (void*)root);
    if (root) {
        /* root should be NULL before any window is opened */
        fprintf(stderr, "  (window already exists from startup)\n");
    } else {
        fprintf(stderr, "  (no root yet, as expected)\n");
    }

    /* --- window_iterate(NULL) --- */
    iter = glk_window_iterate(NULL, &rock);
    fprintf(stderr, "glk_window_iterate(NULL) = %p, rock=%u\n", (void*)iter, rock);

    /* --- window_open --- */
    winid_t win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0);
    ASSERT_NONNULL(win, "glk_window_open should succeed");

    /* --- window_get_root (after open) --- */
    root = glk_window_get_root();
    fprintf(stderr, "glk_window_get_root() after open = %p\n", (void*)root);
    ASSERT_NONNULL(root, "glk_window_get_root should return non-NULL after window_open");

    /* --- window_iterate(NULL) returns the window --- */
    iter = glk_window_iterate(NULL, &rock);
    fprintf(stderr, "glk_window_iterate(NULL) after open = %p, rock=%u\n", (void*)iter, rock);
    ASSERT(iter == win, "glk_window_iterate(NULL) should return the opened window");

    /* --- window_iterate(win) returns NULL --- */
    iter = glk_window_iterate(win, &rock);
    fprintf(stderr, "glk_window_iterate(win) after open = %p\n", (void*)iter);
    ASSERT(iter == NULL, "glk_window_iterate(win) should return NULL (only one window)");

    /* --- request_line_event --- */
    char buf[256];
    memset(buf, 0, sizeof(buf));
    /* This call should succeed - the window is valid */
    glk_request_line_event(win, buf, sizeof(buf), 0);
    /* We can't easily assert on a void function, but it shouldn't crash */

    /* --- fileref_create_by_prompt --- */
    fref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Write, 0);
    fprintf(stderr, "glk_fileref_create_by_prompt(savedgame) = %p\n", (void*)fref);
    ASSERT_NONNULL(fref, "fileref_create_by_prompt for SavedGame should succeed");

    /* --- stream_open_file for writing --- */
    str = glk_stream_open_file(fref, filemode_Write, 0);
    fprintf(stderr, "glk_stream_open_file(write) = %p\n", (void*)str);
    ASSERT_NONNULL(str, "glk_stream_open_file for write should succeed");

    /* Write some data to the stream */
    glk_put_string_stream(str, "FORM");
    glk_put_buffer_stream(str, "\0\0\0\0", 4);  /* placeholder size */
    glk_put_string_stream(str, "IFZS");

    /* --- stream_close --- */
    glk_stream_close(str, NULL);
    fprintf(stderr, "glk_stream_close(write) returned\n");

    /* --- fileref_destroy --- */
    glk_fileref_destroy(fref);
    fprintf(stderr, "glk_fileref_destroy returned\n");

    /* --- window still valid after save-like operations --- */
    root = glk_window_get_root();
    fprintf(stderr, "glk_window_get_root() after save ops = %p\n", (void*)root);
    ASSERT_NONNULL(root, "glk_window_get_root should still return valid window after save ops");
    ASSERT(root == win, "glk_window_get_root should return same window");

    /* --- window_iterate still works --- */
    iter = glk_window_iterate(NULL, &rock);
    fprintf(stderr, "glk_window_iterate(NULL) after save ops = %p\n", (void*)iter);
    ASSERT(iter == win, "glk_window_iterate(NULL) should still return the window after save ops");

    /* --- request_line_event still works --- */
    glk_request_line_event(win, buf, sizeof(buf), 0);
    fprintf(stderr, "glk_request_line_event after save ops succeeded\n");

    /* --- Now test restore-like flow --- */
    /* Create fileref for reading */
    fref = glk_fileref_create_by_prompt(fileusage_SavedGame, filemode_Read, 0);
    fprintf(stderr, "glk_fileref_create_by_prompt(savedgame) for read = %p\n", (void*)fref);
    ASSERT_NONNULL(fref, "fileref_create_by_prompt for SavedGame (read) should succeed");

    /* Open stream for reading */
    str = glk_stream_open_file(fref, filemode_Read, 0);
    fprintf(stderr, "glk_stream_open_file(read) = %p\n", (void*)str);
    ASSERT_NONNULL(str, "glk_stream_open_file for read should succeed");

    /* Read some data */
    char rbuf[4];
    glui32 nread = glk_get_buffer_stream(str, rbuf, 4);
    fprintf(stderr, "glk_get_buffer_stream read %u bytes\n", nread);

    /* --- stream_close --- */
    glk_stream_close(str, NULL);
    fprintf(stderr, "glk_stream_close(read) returned\n");

    /* --- fileref_destroy --- */
    glk_fileref_destroy(fref);
    fprintf(stderr, "glk_fileref_destroy returned\n");

    /* --- window still valid after restore-like operations --- */
    root = glk_window_get_root();
    fprintf(stderr, "glk_window_get_root() after restore ops = %p\n", (void*)root);
    ASSERT_NONNULL(root, "glk_window_get_root should still return valid window after restore ops");
    ASSERT(root == win, "glk_window_get_root should return same window");

    /* --- window_iterate still works --- */
    iter = glk_window_iterate(NULL, &rock);
    fprintf(stderr, "glk_window_iterate(NULL) after restore ops = %p\n", (void*)iter);
    ASSERT(iter == win, "glk_window_iterate(NULL) should still return the window after restore ops");

    /* --- request_line_event still works --- */
    glk_request_line_event(win, buf, sizeof(buf), 0);
    fprintf(stderr, "glk_request_line_event after restore ops succeeded\n");

    /* --- Results --- */
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed ? 1 : 0;
}