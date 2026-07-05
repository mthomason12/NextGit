/* next_window.c — Window lifecycle and query functions
 *
 * Implements the window_t struct and all window-related Glk API functions
 * for a single TextBuffer window.
 *
 * Ownership (from docs/glk-object-lifecycle.md):
 *   window_t.str      — OWNED (created/destroyed with window)
 *   window_t.echostr  — NOT OWNED (set by game, not closed on window close)
 *   window_t.linebuf  — NOT OWNED (points into VM memory)
 *
 * Multi-window support is NOT implemented.
 * Input handling (line/char requests) is NOT implemented.
 * Image support is NOT implemented.
 */

#include <stdio.h>
#include "nextglk_internal.h"
#include "nextglk.h"

/* -------------------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------------------- */

window_t *gli_mainwin = NULL;

/* -------------------------------------------------------------------------
 * gli_new_window — Allocate and initialise a window
 *
 * Allocates a window_t, creates the window's output stream via
 * gli_new_stream(), sets the stream's back-pointer to the window,
 * registers the window with the dispatch layer (if callback available),
 * and sets gli_mainwin.
 *
 * Parameters:
 *   rock — game-supplied rock value (stored but not used yet)
 *
 * Returns:
 *   Pointer to the newly allocated window, or NULL on allocation failure.
 * ------------------------------------------------------------------------- */

window_t *gli_new_window(glui32 rock)
{
    window_t *win;

    (void)rock;  /* rock value is stored for future use */

    win = (window_t *)malloc(sizeof(window_t));
    if (!win)
        return NULL;

    /* Zero-initialise all fields */
    win->disprock.num = 0;
    win->str = NULL;
    win->echostr = NULL;
    win->line_request = 0;
    win->line_request_uni = 0;
    win->char_request = 0;
    win->char_request_uni = 0;
    win->linebuf = NULL;
    win->linebuflen = 0;
    win->inarrayrock.num = 0;

    /* Create the window's output stream:
     *   type     = strtype_Window
     *   readable = 0 (windows are write-only via stream)
     *   writable = 1
     *   rock     = 0 (no game rock for the stream)
     */
    win->str = gli_new_stream(strtype_Window, 0, 1, 0);
    if (!win->str) {
        free(win);
        return NULL;
    }

    /* Set the stream's back-pointer to this window */
    win->str->win = win;

    /* Register with dispatch layer if callback is available */
    if (gli_register_obj)
        win->disprock = (*gli_register_obj)(win, gidisp_Class_Window);

    fprintf(stderr, "WINDOW_REG: win=%p disprock.ptr=%p disprock.num=%u\n",
        (void*)win, win->disprock.ptr, win->disprock.num);

    /* Set as the single main window */
    gli_mainwin = win;

    return win;
}

/* -------------------------------------------------------------------------
 * gli_delete_window — Destroy a window and its owned stream
 *
 * Fills the stream result (if requested), unregisters any input array
 * from the dispatch layer, unregisters the window from the dispatch layer,
 * deletes the owned window stream via gli_delete_stream(), frees the
 * window, and sets gli_mainwin to NULL.
 *
 * Parameters:
 *   win    — the window to destroy (must not be NULL)
 *   result — optional pointer to receive the window stream's final
 *            readcount/writecount
 * ------------------------------------------------------------------------- */

void gli_delete_window(window_t *win, stream_result_t *result)
{
    if (!win)
        return;

    /* Fill result from the window stream before destroying it */
    gli_stream_fill_result(win->str, result);

    /* Unregister input array if one was registered */
    if (win->linebuf) {
        /* The input array would be unregistered here when dispatch-layer
         * array registration is wired in Commit 3.
         * Currently a no-op placeholder. */
        /* gli_unregister_arr(win->linebuf, win->linebuflen, "&+#!Cn",
         *     win->inarrayrock); */
    }

    /* Unregister window from dispatch layer */
    if (gli_unregister_obj)
        (*gli_unregister_obj)(win, gidisp_Class_Window, win->disprock);

    /* Destroy the owned window stream */
    gli_delete_stream(win->str, NULL);

    /* Echo stream is NOT owned — do not close it */
    win->echostr = NULL;

    /* Free the window */
    free(win);

    /* Clear the main window global */
    gli_mainwin = NULL;
}

/* -------------------------------------------------------------------------
 * glk_window_open — Open a window
 *
 * The minimum implementation supports a single TextBuffer window only.
 * If gli_mainwin is already set, this function fails gracefully (returns
 * NULL) rather than crashing.
 *
 * Parameters:
 *   split   — window to split (ignored; single-window only)
 *   method  — split method (ignored)
 *   size    — split size (ignored)
 *   wintype — type of window to create (must be wintype_TextBuffer)
 *   rock    — game-supplied rock value
 *
 * Returns:
 *   The window identifier, or NULL if a window already exists or the
 *   window type is not supported.
 * ------------------------------------------------------------------------- */

winid_t glk_window_open(winid_t split, glui32 method, glui32 size,
    glui32 wintype, glui32 rock)
{
    (void)split;   /* single-window only */
    (void)method;  /* ignored */
    (void)size;    /* ignored */
    (void)rock;

    /* Single-window constraint: only one window allowed */
    if (gli_mainwin != NULL)
        return NULL;

    /* Only TextBuffer windows are supported */
    if (wintype != wintype_TextBuffer)
        return NULL;

    return gli_new_window(0);
}

/* -------------------------------------------------------------------------
 * glk_window_close — Close a window
 *
 * Validates that the window is the main window, then destroys it.
 *
 * Parameters:
 *   win    — the window to close
 *   result — optional pointer to receive stream readcount/writecount
 * ------------------------------------------------------------------------- */

void glk_window_close(winid_t win, stream_result_t *result)
{
    window_t *w = (window_t *)win;

    /* Validate that this is the main window */
    if (w != gli_mainwin)
        return;

    gli_delete_window(w, result);
}

/* -------------------------------------------------------------------------
 * glk_window_get_root — Return the root window
 *
 * In single-window mode, this is always gli_mainwin.
 * ------------------------------------------------------------------------- */

winid_t glk_window_get_root(void)
{
    fprintf(stderr, "DEBUG glk_window_get_root: returning %p (gli_mainwin=%p)\n",
        (void*)gli_mainwin, (void*)gli_mainwin);
    return gli_mainwin;
}

/* -------------------------------------------------------------------------
 * glk_window_get_type — Return the window type
 *
 * The single supported window type is wintype_TextBuffer.
 * ------------------------------------------------------------------------- */

glui32 glk_window_get_type(winid_t win)
{
    (void)win;  /* only TextBuffer exists */
    return wintype_TextBuffer;
}

/* -------------------------------------------------------------------------
 * glk_window_get_size — Return the window size
 *
 * Returns a fixed size of 80 columns by 25 rows.
 * This is a stub — the real implementation will query the platform
 * screen dimensions.
 * ------------------------------------------------------------------------- */

void glk_window_get_size(winid_t win, glui32 *widthptr, glui32 *heightptr)
{
    (void)win;

    if (widthptr)
        *widthptr = 80;

    if (heightptr)
        *heightptr = 25;
}

/* -------------------------------------------------------------------------
 * glk_window_get_stream — Return the window's output stream
 * ------------------------------------------------------------------------- */

strid_t glk_window_get_stream(winid_t win)
{
    window_t *w = (window_t *)win;

    if (!w)
        return NULL;

    return w->str;
}

/* -------------------------------------------------------------------------
 * glk_window_get_sibling — Return the window's sibling
 *
 * In single-window mode, there are no siblings. Always returns NULL.
 * ------------------------------------------------------------------------- */

winid_t glk_window_get_sibling(winid_t win)
{
    (void)win;
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_window_iterate — Iterate over all active windows
 *
 * Single-window iteration:
 *   - If win is NULL, returns gli_mainwin (or NULL if no window).
 *   - If win is gli_mainwin, returns NULL (no more windows).
 *   - Otherwise returns NULL (invalid window).
 *
 * If rockptr is non-NULL, writes the window's rock value into *rockptr.
 * ------------------------------------------------------------------------- */

winid_t glk_window_iterate(winid_t win, glui32 *rockptr)
{
    window_t *w = (window_t *)win;

    if (w == NULL) {
        /* First call: return main window if it exists */
        if (rockptr)
            *rockptr = 0;

        return gli_mainwin;
    }
    else if (w == gli_mainwin) {
        /* Already returned the only window; no more */
        if (rockptr)
            *rockptr = 0;

        return NULL;
    }
    else {
        /* Invalid window */
        if (rockptr)
            *rockptr = 0;

        return NULL;
    }
}

/* -------------------------------------------------------------------------
 * glk_window_clear — Clear the window
 *
 * Delegates to the platform screen layer.
 * ------------------------------------------------------------------------- */

void glk_window_clear(winid_t win)
{
    (void)win;
    nextglk_screen_clear();
}

/* -------------------------------------------------------------------------
 * glk_set_window — Set the current output window
 *
 * Sets gli_currentstr to the window's output stream.
 * Subsequent output functions (glk_put_char, etc.) will write to
 * this window.
 * ------------------------------------------------------------------------- */

void glk_set_window(winid_t win)
{
    window_t *w = (window_t *)win;

    if (w)
        gli_currentstr = w->str;
}

/* -------------------------------------------------------------------------
 * glk_window_get_rock — Return the window's rock value
 *
 * Stub — always returns 0.
 * Required by the dispatch function table.
 * ------------------------------------------------------------------------- */

glui32 glk_window_get_rock(winid_t win)
{
    (void)win;
    return 0;
}

/* -------------------------------------------------------------------------
 * glk_window_get_parent — Return the window's parent
 *
 * In single-window mode, there is no parent. Always returns NULL.
 * ------------------------------------------------------------------------- */

winid_t glk_window_get_parent(winid_t win)
{
    (void)win;
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_window_move_cursor — Move the cursor within the window
 *
 * Stub — cursor movement is only relevant for TextGrid and Graphics
 * window types. The TextBuffer window type ignores cursor positioning.
 * ------------------------------------------------------------------------- */

void glk_window_move_cursor(winid_t win, glui32 xpos, glui32 ypos)
{
    (void)win;
    (void)xpos;
    (void)ypos;
    /* No-op: cursor positioning is not applicable to TextBuffer windows */
}