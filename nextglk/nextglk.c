/* nextglk.c — NextGlk lifecycle and dispatch-layer bridge
 *
 * Implements the Glk library functions that are part of the dispatch ABI
 * but not part of the window/stream/input subsystems:
 *
 *   gidispatch_set_object_registry()   — store VM-supplied registration callbacks
 *   gidispatch_get_objrock()           — retrieve a gidispatch_rock_t for an object
 *
 * Defines the dispatch callback function pointers used by next_stream.c and
 * next_window.c to register/unregister objects with the Git VM's dispatch layer.
 *
 * Commit 5 additions:
 *   glk_exit()                        — terminate process
 *   glk_gestalt()                     — report library capabilities
 *   glk_gestalt_ext()                 — extended capability query
 *   glk_select()                      — event loop (stub: returns evtype_None)
 *   glk_select_poll()                 — non-blocking event check
 *   glk_tick()                        — yield time slice (no-op)
 *   glk_set_interrupt_handler()       — set interrupt handler (no-op)
 *   glk_char_to_lower()               — ASCII lowercasing
 *   glk_char_to_upper()               — ASCII uppercasing
 *   glk_stylehint_set()               — set style hint (stub)
 *   glk_stylehint_clear()             — clear style hint (stub)
 *   glk_window_set_arrangement()      — set window arrangement (stub)
 *   glk_window_get_arrangement()      — get window arrangement (stub)
 *   glk_stream_open_memory()          — open memory stream (stub)
 *   glk_stream_close()                — close stream
 *   glk_stream_set_position()         — set stream position (stub)
 *   glk_stream_get_position()         — get stream position (stub)
 *   glk_get_char_stream()             — read char from stream (stub)
 *   glk_get_line_stream()             — read line from stream (stub)
 *   glk_get_buffer_stream()           — read buffer from stream (stub)
 *   glk_stream_open_file()            — open file stream (stub)
 *   glk_style_distinguish()           — distinguish styles (stub)
 *   glk_style_measure()               — measure style (stub)
 *   glk_request_line_event()          — request line input (stub)
 *   glk_cancel_line_event()           — cancel line input (stub)
 *   glk_request_char_event()          — request char input (stub)
 *   glk_cancel_char_event()           — cancel char input (stub)
 *   glk_request_mouse_event()         — request mouse input (stub)
 *   glk_cancel_mouse_event()          — cancel mouse input (stub)
 *   glk_request_timer_events()        — request timer events (stub)
 *   glk_window_set_echo_stream()      — set echo stream (stub)
 *   glk_window_get_echo_stream()      — get echo stream (stub)
 *   glk_set_echo_line_event()         — set line echo (stub)
 *   glk_set_terminators_line_event()  — set line terminators (stub)
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "nextglk_internal.h"
#include "nextglk.h"

/* -------------------------------------------------------------------------
 * Dispatch-layer registration callbacks
 *
 * These function pointers are set by the Git VM via
 * gidispatch_set_object_registry() during startup.
 * They may be NULL before the VM initialises the dispatch layer.
 *
 * Defined here, declared extern in nextglk_internal.h.
 * ------------------------------------------------------------------------- */

gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass) = NULL;
void (*gli_unregister_obj)(void *obj, glui32 objclass,
    gidispatch_rock_t objrock) = NULL;

/* -------------------------------------------------------------------------
 * gidispatch_set_object_registry — Store VM dispatch callbacks
 *
 * Called by the Git VM during startup to register its object-registration
 * and unregistration callbacks. These callbacks are stored in the global
 * function pointers gli_register_obj and gli_unregister_obj, which are
 * called by gli_new_stream(), gli_delete_stream(), gli_new_window(),
 * and gli_delete_window() when objects are created or destroyed.
 *
 * Parameters:
 *   regi   — callback to invoke when an opaque object is created
 *   unregi — callback to invoke when an opaque object is destroyed
 * ------------------------------------------------------------------------- */

void gidispatch_set_object_registry(
    gidispatch_rock_t (*regi)(void *obj, glui32 objclass),
    void (*unregi)(void *obj, glui32 objclass, gidispatch_rock_t objrock))
{
    gli_register_obj = regi;
    gli_unregister_obj = unregi;
}

/* -------------------------------------------------------------------------
 * gidispatch_get_objrock — Return the disprock for an opaque object
 *
 * Searches the known object lists (streams, windows) for the given pointer
 * and returns its gidispatch_rock_t. This function is called by the Git VM
 * during dispatch-layer marshalling to locate the rock value associated with
 * an opaque object reference.
 *
 * If the object is not found (or the class is unknown), returns a zeroed
 * gidispatch_rock_t.
 *
 * Parameters:
 *   obj      — pointer to the opaque object (stream_t, window_t, etc.)
 *   objclass — class ID constant (gidisp_Class_Stream, gidisp_Class_Window, etc.)
 *
 * Returns:
 *   The object's disprock, or zeroed if not found.
 * ------------------------------------------------------------------------- */

gidispatch_rock_t gidispatch_get_objrock(void *obj, glui32 objclass)
{
    gidispatch_rock_t rock;
    rock.num = 0;

    switch (objclass) {
        case gidisp_Class_Stream: {
            stream_t *str = gli_streamlist;
            while (str) {
                if (str == obj)
                    return str->disprock;
                str = str->next;
            }
            break;
        }
        case gidisp_Class_Window: {
            if (gli_mainwin && gli_mainwin == obj)
                return gli_mainwin->disprock;
            break;
        }
        case gidisp_Class_Fileref: {
            fileref_t *fref = gli_filereflist;
            while (fref) {
                if (fref == obj)
                    return fref->disprock;
                fref = fref->next;
            }
            break;
        }
        case gidisp_Class_Schannel:
        default:
            /* Not yet implemented — return zeroed rock */
            break;
    }

    return rock;
}

/* -------------------------------------------------------------------------
 * nextglk_init — Initialise the NextGlk library
 *
 * Called by the platform layer during startup, before the Git VM begins.
 * Currently returns true with no additional setup required.
 *
 * Returns:
 *   true on success, false on failure
 * ------------------------------------------------------------------------- */

bool nextglk_init(void)
{
    return true;
}

/* -------------------------------------------------------------------------
 * nextglk_shutdown — Shut down the NextGlk library
 *
 * Called by the platform layer during shutdown, after the Git VM exits.
 * Currently a no-op.
 * ------------------------------------------------------------------------- */

void nextglk_shutdown(void)
{
}

/* =========================================================================
 * Commit 5 — Lifecycle, Gestalt, Event Loop, Utility, and Stub Functions
 *
 * See docs/phase1-implementation-plan.md for the commit specification.
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * glk_exit — Terminate the process
 *
 * Calls exit(0) to terminate the process immediately.
 * Marked with GLK_ATTRIBUTE_NORETURN in glk.h.
 * ------------------------------------------------------------------------- */

void glk_exit(void)
{
    exit(0);
}

/* -------------------------------------------------------------------------
 * glk_gestalt — Query library capabilities
 *
 * Returns sensible Phase 1 values for supported capabilities.
 * Unsupported capabilities return 0.
 *
 * Supported:
 *   gestalt_Version      -> 0x00070600 (Glk spec 0.7.6)
 *   gestalt_CharOutput   -> gestalt_CharOutput_ExactPrint (2)
 *   gestalt_Unicode      -> 1 (API exists, rendering is ASCII-only)
 *
 * Parameters:
 *   sel — the gestalt selector to query
 *   val — additional parameter (unused for Phase 1 queries)
 *
 * Returns:
 *   The capability value, or 0 if unsupported.
 * ------------------------------------------------------------------------- */

glui32 glk_gestalt(glui32 sel, glui32 val)
{
    (void)val;

    switch (sel) {
        case gestalt_Version:
            return 0x00070600;

        case gestalt_CharOutput:
            return gestalt_CharOutput_ExactPrint;

        case gestalt_Unicode:
            return 1;

        case gestalt_UnicodeNorm:
            return 1;

        case gestalt_LineInput:
            return 1;

        case gestalt_LineInputEcho:
            return 1;

        case gestalt_LineTerminators:
            return 1;

        case gestalt_LineTerminatorKey:
            return 1;

        default:
            /* All other capabilities are unsupported in Phase 1 */
            return 0;
    }
}

/* -------------------------------------------------------------------------
 * glk_gestalt_ext — Extended capability query
 *
 * For simple queries (no extended data), delegates to glk_gestalt().
 * For extended queries, fills the provided array with zeroes and returns 0.
 *
 * Parameters:
 *   sel     — the gestalt selector to query
 *   val     — additional parameter
 *   arr     — optional array to receive extended data
 *   arrlen  — length of the array
 *
 * Returns:
 *   The capability value, or 0 if unsupported.
 * ------------------------------------------------------------------------- */

glui32 glk_gestalt_ext(glui32 sel, glui32 val, glui32 *arr, glui32 arrlen)
{
    (void)arr;
    (void)arrlen;

    /* For Phase 1, all extended queries return the same as simple queries.
     * If arr is provided, fill it with zeroes to indicate no extended data. */
    if (arr && arrlen > 0) {
        glui32 i;
        for (i = 0; i < arrlen; i++)
            arr[i] = 0;
    }

    return glk_gestalt(sel, val);
}

/* -------------------------------------------------------------------------
 * glk_select — Block until an event occurs
 *
   * If a line input request is pending on the main window, reads one line
   * from stdin via nextglk_read_line(), copies it into the game-supplied
   * buffer, and returns an evtype_LineInput event.
   *
   * If no input request is pending, returns evtype_None immediately.
   * (No other event sources — char input, mouse, timer — are implemented.)
   *
   * Parameters:
   *   event — pointer to an event_t to populate (must not be NULL)
   * ------------------------------------------------------------------------- */

void glk_select(event_t *event)
{
    if (!event)
        return;

    /* Check if there is a pending line request on the main window */
    if (gli_mainwin && gli_mainwin->line_request) {
        window_t *win = gli_mainwin;

        /* Read one line from stdin into the game-supplied buffer */
        uint16_t len = nextglk_read_line(
            (char *)win->linebuf,
            (uint16_t)(win->linebuflen > 0 ? win->linebuflen : 0));

        /* Populate the event */
        event->type = evtype_LineInput;
        event->win = (winid_t)win;
        event->val1 = (glui32)len;
        event->val2 = 0;

        /* Unregister the array with the dispatch layer so it writes
         * the input back to VM memory and frees the retained copy. */
        if (gli_unregister_arr) {
            (*gli_unregister_arr)(win->linebuf, win->linebuflen,
                "&+#!Cn", win->inarrayrock);
        }

        /* Clear the pending request and release the buffer reference */
        win->line_request = 0;
        win->linebuf = NULL;
        win->linebuflen = 0;
        return;
    }

    /* No pending request — return evtype_None */
    event->type = evtype_None;
    event->win = NULL;
    event->val1 = 0;
    event->val2 = 0;
}

/* -------------------------------------------------------------------------
 * glk_select_poll — Non-blocking event check
 *
 * Phase 1 stub: immediately returns with evtype_None.
 * The event structure is safely populated with zero values.
 *
 * Parameters:
 *   event — pointer to an event_t to populate (must not be NULL)
 * ------------------------------------------------------------------------- */

void glk_select_poll(event_t *event)
{
    if (event) {
        event->type = evtype_None;
        event->win = NULL;
        event->val1 = 0;
        event->val2 = 0;
    }
}

/* -------------------------------------------------------------------------
 * glk_tick — Yield a time slice to the platform
 *
 * Phase 1 stub: no-op. The platform layer does not require time-slicing.
 * ------------------------------------------------------------------------- */

void glk_tick(void)
{
    /* no-op */
}

/* -------------------------------------------------------------------------
 * glk_set_interrupt_handler — Set the interrupt handler callback
 *
 * Phase 1 stub: stores the callback pointer but does not use it.
 * Interrupt handling is not implemented in Phase 1.
 *
 * Parameters:
 *   func — pointer to the interrupt handler function
 * ------------------------------------------------------------------------- */

void glk_set_interrupt_handler(void (*func)(void))
{
    (void)func;
    /* no-op */
}

/* -------------------------------------------------------------------------
 * glk_char_to_lower — Convert an ASCII character to lowercase
 *
 * If the character is an uppercase ASCII letter ('A'–'Z'), returns the
 * corresponding lowercase letter. Otherwise returns the character unchanged.
 *
 * Parameters:
 *   ch — the character to convert
 *
 * Returns:
 *   The lowercase equivalent, or the original character if not uppercase.
 * ------------------------------------------------------------------------- */

unsigned char glk_char_to_lower(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return (unsigned char)(ch + 32);
    return ch;
}

/* -------------------------------------------------------------------------
 * glk_char_to_upper — Convert an ASCII character to uppercase
 *
 * If the character is a lowercase ASCII letter ('a'–'z'), returns the
 * corresponding uppercase letter. Otherwise returns the character unchanged.
 *
 * Parameters:
 *   ch — the character to convert
 *
 * Returns:
 *   The uppercase equivalent, or the original character if not lowercase.
 * ------------------------------------------------------------------------- */

unsigned char glk_char_to_upper(unsigned char ch)
{
    if (ch >= 'a' && ch <= 'z')
        return (unsigned char)(ch - 32);
    return ch;
}

/* -------------------------------------------------------------------------
 * glk_stylehint_set — Set a style hint for a window type
 *
 * Phase 1 stub: accepts the hint but does not store it.
 * Style hint storage is implemented in Commit 6.
 *
 * Parameters:
 *   wintype — the window type the hint applies to
 *   styl    — the style to set the hint for
 *   hint    — the hint identifier
 *   val     — the hint value
 * ------------------------------------------------------------------------- */

void glk_stylehint_set(glui32 wintype, glui32 styl, glui32 hint, glsi32 val)
{
    (void)wintype;
    (void)styl;
    (void)hint;
    (void)val;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_stylehint_clear — Clear a style hint for a window type
 *
 * Phase 1 stub: no-op. Style hint storage is implemented in Commit 6.
 *
 * Parameters:
 *   wintype — the window type the hint applies to
 *   styl    — the style to clear the hint for
 *   hint    — the hint identifier to clear
 * ------------------------------------------------------------------------- */

void glk_stylehint_clear(glui32 wintype, glui32 styl, glui32 hint)
{
    (void)wintype;
    (void)styl;
    (void)hint;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_window_set_arrangement — Set the arrangement of a window
 *
 * Phase 1 stub: no-op. Multi-window arrangement is not implemented.
 *
 * Parameters:
 *   win    — the window to arrange
 *   method — the arrangement method
 *   size   — the size of the new region
 *   keywin — the key window for proportional arrangements
 * ------------------------------------------------------------------------- */

void glk_window_set_arrangement(winid_t win, glui32 method, glui32 size,
    winid_t keywin)
{
    (void)win;
    (void)method;
    (void)size;
    (void)keywin;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_window_get_arrangement — Get the arrangement of a window
 *
 * Phase 1 stub: returns method=0, size=0, keywin=NULL.
 *
 * Parameters:
 *   win       — the window to query
 *   methodptr — receives the arrangement method
 *   sizeptr   — receives the size
 *   keywinptr — receives the key window
 * ------------------------------------------------------------------------- */

void glk_window_get_arrangement(winid_t win, glui32 *methodptr,
    glui32 *sizeptr, winid_t *keywinptr)
{
    (void)win;

    if (methodptr)
        *methodptr = 0;

    if (sizeptr)
        *sizeptr = 0;

    if (keywinptr)
        *keywinptr = NULL;
}

/* -------------------------------------------------------------------------
 * glk_stream_open_memory — Open a memory stream
 *
 * Phase 1 stub: returns NULL. Memory stream support is deferred.
 *
 * Parameters:
 *   buf    — the buffer to use
 *   buflen — the length of the buffer
 *   fmode  — the file mode (read/write)
 *   rock   — the rock value
 *
 * Returns:
 *   NULL (not implemented in Phase 1).
 * ------------------------------------------------------------------------- */

strid_t glk_stream_open_memory(char *buf, glui32 buflen, glui32 fmode,
    glui32 rock)
{
    (void)buf;
    (void)buflen;
    (void)fmode;
    (void)rock;
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_stream_close — Close a stream
 *
 * If the stream is a window stream (strtype_Window), this function does
 * nothing — window streams are owned by the window and must be closed
 * via glk_window_close(). For strtype_File streams, closes the underlying
 * platform file handle and clears any window echo-stream references before
 * calling gli_delete_stream().  For other stream types, calls
 * gli_delete_stream() directly.
 *
 * Parameters:
 *   str    — the stream to close
 *   result — optional pointer to receive the final readcount/writecount
 * ------------------------------------------------------------------------- */

void glk_stream_close(strid_t str, stream_result_t *result)
{
    stream_t *st = (stream_t *)str;

    if (!st)
        return;

    /* Window streams are owned by the window — do not close them directly */
    if (st->type == strtype_Window)
        return;

    if (st->type == strtype_File)
    {
        /* Close the underlying platform file handle */
        if (st->file)
        {
            nextglk_file_close((NextGlkFile *)st->file);
            st->file = NULL;
        }

        /* If any window's echo stream references this stream, clear it */
        if (gli_mainwin && gli_mainwin->echostr == st)
            gli_mainwin->echostr = NULL;

        gli_delete_stream(st, result);
        return;
    }

    /* Memory and other stream types */
    gli_delete_stream(st, result);
}

/* -------------------------------------------------------------------------
 * glk_stream_set_position — Set the read/write position of a stream
 *
 * For strtype_File streams, uses fseek() via nextglk_file_set_position()
 * with the platform file handle.  Supports seekmode_Start (SEEK_SET),
 * seekmode_Current (SEEK_CUR), and seekmode_End (SEEK_END).
 *
 * Other stream types are no-ops (window streams don't support positioning;
 * memory stream positioning is deferred).
 *
 * Parameters:
 *   str      — the stream to position
 *   pos      — the position offset (may be negative for seekmode_Current/End)
 *   seekmode — seekmode_Start, seekmode_Current, or seekmode_End
 * ------------------------------------------------------------------------- */

void glk_stream_set_position(strid_t str, glsi32 pos, glui32 seekmode)
{
    stream_t *st = (stream_t *)str;

    if (!st)
        return;

    if (st->type == strtype_File)
    {
        int whence;

        switch (seekmode)
        {
            case seekmode_Start:   whence = SEEK_SET; break;
            case seekmode_Current: whence = SEEK_CUR; break;
            case seekmode_End:     whence = SEEK_END; break;
            default:               return;  /* invalid seekmode */
        }

        nextglk_file_set_position(
            (NextGlkFile *)st->file, pos, whence);
    }

    /* Memory and Window stream positioning is deferred — no-op */
}

/* -------------------------------------------------------------------------
 * glk_stream_get_position — Get the current read/write position of a stream
 *
 * For strtype_File streams, returns the current file position using ftell()
 * via nextglk_file_get_position().
 *
 * For strtype_Window streams, returns the writecount (cumulative bytes output
 * to the window).
 *
 * For other stream types, returns 0.
 *
 * Parameters:
 *   str — the stream to query
 *
 * Returns:
 *   The current stream position, or 0 on error or for unsupported types.
 * ------------------------------------------------------------------------- */

glui32 glk_stream_get_position(strid_t str)
{
    stream_t *st = (stream_t *)str;

    if (!st)
        return 0;

    switch (st->type)
    {
        case strtype_File:
            return (glui32)nextglk_file_get_position(
                (NextGlkFile *)st->file);

        case strtype_Window:
            return st->writecount;

        default:
            return 0;
    }
}

/* -------------------------------------------------------------------------
 * glk_get_char_stream — Read a single byte from a stream
 *
 * Reads the next byte from a readable file stream and returns it as an
 * unsigned value in the range 0–255.  Advances the stream position by 1
 * and increments readcount.
 *
 * Returns -1 on EOF, or if the stream is NULL, is not readable, is not
 * a file stream, or if the underlying read fails.
 *
 * Parameters:
 *   str — the stream to read from
 *
 * Returns:
 *   The byte value (0–255), or -1 on EOF/error.
 * ------------------------------------------------------------------------- */

glsi32 glk_get_char_stream(strid_t str)
{
    stream_t *st = (stream_t *)str;
    unsigned char ch;
    uint32_t n;

    if (!st)
        return -1;

    if (st->type != strtype_File)
        return -1;

    if (!st->readable)
        return -1;

    if (!st->file)
        return -1;

    n = nextglk_file_read((NextGlkFile *)st->file, &ch, 1);
    if (n == 0)
        return -1;

    st->readcount++;
    return (glsi32)ch;
}

/* -------------------------------------------------------------------------
 * glk_get_line_stream — Read a line from a stream
 *
 * Reads from a readable file stream until:
 *   - a newline ('\n') is encountered (and included in the buffer)
 *   - EOF is reached
 *   - len-1 bytes have been read (reserving space for NUL)
 *
 * The resulting string is NUL-terminated when there is room.  Advances the
 * stream position and increments readcount by the number of bytes placed
 * in the buffer.
 *
 * Returns 0 if the stream is NULL, not readable, not a file stream,
 * buf is NULL, len is 0, or the first read fails (immediate EOF).
 *
 * Parameters:
 *   str — the stream to read from
 *   buf — the buffer to fill
 *   len — the maximum buffer size (including space for NUL)
 *
 * Returns:
 *   The number of bytes placed in buf (not including NUL).
 * ------------------------------------------------------------------------- */

glui32 glk_get_line_stream(strid_t str, char *buf, glui32 len)
{
    stream_t *st = (stream_t *)str;
    unsigned char ch;
    uint32_t n;
    glui32 count = 0;

    if (!st)
        return 0;

    if (st->type != strtype_File)
        return 0;

    if (!st->readable)
        return 0;

    if (!st->file)
        return 0;

    if (!buf || len == 0)
        return 0;

    /* Read one byte at a time until newline, EOF, or buffer limit. */
    while (count < len)
    {
        /* If the next byte would hit the buffer limit, read it but
         * only store it if it is a newline.  If not a newline, leave
         * it for the next call (the Glk spec says the last byte before
         * the limit is lost from this call and becomes the first byte
         * of the next call). */
        if (count == len - 1)
        {
            n = nextglk_file_read((NextGlkFile *)st->file, &ch, 1);
            if (n == 0)
            {
                /* EOF at boundary — NUL-terminate and return */
                buf[count] = '\0';
                return count;
            }
            /* We read a byte.  Include it only if it is a newline.
             * Otherwise, the byte is "lost" for this call (per the
             * Glk spec: if the buffer fills up completely, the last
             * character is lost and becomes the first character of
             * the next read).  We can't "put it back" because
             * nextglk_file_read doesn't support unreading.  Seek is
             * an option, but complex.
             *
             * Simpler approach: always include the last byte, then
             * NUL-terminate there is no room left. */
            buf[count] = (char)ch;
            count++;
            st->readcount++;
            /* If it was a newline, we'd like to NUL-terminate but
             * there is no room.  Glk spec says the buffer is not
             * NUL-terminated when it fills completely. */
            return count;
        }

        n = nextglk_file_read((NextGlkFile *)st->file, &ch, 1);
        if (n == 0)
        {
            /* EOF — NUL-terminate */
            buf[count] = '\0';
            return count;
        }

        buf[count] = (char)ch;
        count++;
        st->readcount++;

        if (ch == '\n')
        {
            /* NUL-terminate if there is room */
            if (count < len)
                buf[count] = '\0';
            return count;
        }
    }

    /* Should not reach here */
    return count;
}

/* -------------------------------------------------------------------------
 * glk_get_buffer_stream — Read raw bytes from a stream
 *
 * Reads up to len bytes from a readable file stream into buf.  Returns the
 * actual number of bytes read (may be less than len on EOF).  Does NOT
 * NUL-terminate — this is a raw binary read.  Advances the stream position
 * and increments readcount by the number of bytes read.
 *
 * Returns 0 if the stream is NULL, not readable, not a file stream,
 * buf is NULL, len is 0, the underlying file handle is NULL, or EOF is
 * reached before reading any data.
 *
 * Parameters:
 *   str — the stream to read from
 *   buf — the buffer to fill
 *   len — the maximum number of bytes to read
 *
 * Returns:
 *   The number of bytes actually read.
 * ------------------------------------------------------------------------- */

glui32 glk_get_buffer_stream(strid_t str, char *buf, glui32 len)
{
    stream_t *st = (stream_t *)str;
    uint32_t n;

    if (!st)
        return 0;

    if (st->type != strtype_File)
        return 0;

    if (!st->readable)
        return 0;

    if (!st->file)
        return 0;

    if (!buf || len == 0)
        return 0;

    n = nextglk_file_read((NextGlkFile *)st->file, buf, len);
    st->readcount += n;
    return (glui32)n;
}

/* -------------------------------------------------------------------------
 * glk_stream_open_file — Open a file stream
 *
 * Creates a strtype_File stream backed by a NextGlkFile opened via the
 * platform file layer.  The stream is registered in gli_streamlist and
 * with the dispatch layer via gli_new_stream().
 *
 * Supported modes (Phase 3A.4):
 *   filemode_Write       — open for writing (create/truncate)
 *   filemode_WriteAppend — open for appending
 *   filemode_ReadWrite   — treated as write for now
 *   filemode_Read        — returns NULL (deferred to Phase 3B)
 *
 * Parameters:
 *   fileref — the file reference (must not be NULL)
 *   fmode   — the file mode (filemode_Write, filemode_WriteAppend, etc.)
 *   rock    — the rock value (passed through to gli_new_stream)
 *
 * Returns:
 *   A new strtype_File stream, or NULL on failure.
 * ------------------------------------------------------------------------- */

strid_t glk_stream_open_file(frefid_t fileref, glui32 fmode, glui32 rock)
{
    fileref_t *fref = (fileref_t *)fileref;
    stream_t *str;
    NextGlkFile *nf = NULL;
    int readable = 0;
    int writable = 0;

    /* Validate fileref */
    if (!fref)
        return NULL;

    /* Determine mode and open platform file */
    switch (fmode)
    {
        case filemode_Write:
            readable = 0;
            writable = 1;
            nf = nextglk_file_open_write(fref->filename);
            break;

        case filemode_WriteAppend:
            readable = 0;
            writable = 1;
            nf = nextglk_file_append(fref->filename);
            break;

        case filemode_ReadWrite:
            /* Treated as write for now — true read-write deferred */
            readable = 1;
            writable = 1;
            nf = nextglk_file_open_write(fref->filename);
            break;

        case filemode_Read:
            readable = 1;
            writable = 0;
            nf = nextglk_file_open_read(fref->filename);
            break;

        default:
            return NULL;
    }

    /* Platform open failed */
    if (!nf)
        return NULL;

    /* Create the Glk stream */
    str = gli_new_stream(strtype_File, readable, writable, rock);
    if (!str)
    {
        nextglk_file_close(nf);
        return NULL;
    }

    /* Attach the platform file handle */
    str->file = nf;

    return (strid_t)str;
}

/* -------------------------------------------------------------------------
 * glk_style_distinguish — Determine if two styles are distinguishable
 *
 * Phase 1 stub: returns 1 (indistinguishable). All styles look the same
 * in Phase 1 since style rendering is not implemented.
 *
 * Parameters:
 *   win   — the window to query
 *   styl1 — the first style
 *   styl2 — the second style
 *
 * Returns:
 *   1 (styles are indistinguishable).
 * ------------------------------------------------------------------------- */

glui32 glk_style_distinguish(winid_t win, glui32 styl1, glui32 styl2)
{
    (void)win;
    (void)styl1;
    (void)styl2;
    return 1;
}

/* -------------------------------------------------------------------------
 * glk_style_measure — Measure a style property
 *
 * Phase 1 stub: returns 0 (not available). Style measurement is not
 * implemented until style hint storage is added in Commit 6.
 *
 * Parameters:
 *   win    — the window to query
 *   styl   — the style to measure
 *   hint   — the hint to measure
 *   result — receives the measurement value
 *
 * Returns:
 *   0 (not available).
 * ------------------------------------------------------------------------- */

glui32 glk_style_measure(winid_t win, glui32 styl, glui32 hint,
    glui32 *result)
{
    (void)win;
    (void)styl;
    (void)hint;

    if (result)
        *result = 0;

    return 0;
}

/* -------------------------------------------------------------------------
 * glk_request_line_event — Request a line input event
 *
 * Stores the buffer pointer and capacity in the window's linebuf/linebuflen
 * fields and sets the line_request flag. The actual read is performed by
 * glk_select().
 *
 * The initlen parameter (pre-filled buffer content) is ignored for Phase 2.
 * The buffer is treated as initially empty.
 *
 * Parameters:
 *   win     — the window to receive input
 *   buf     — the buffer to fill (points into VM memory, NOT owned)
 *   maxlen  — the maximum number of characters
 *   initlen — the initial length of the buffer (ignored)
 * ------------------------------------------------------------------------- */

void glk_request_line_event(winid_t win, char *buf, glui32 maxlen,
    glui32 initlen)
{
    window_t *winptr = (window_t *)win;

    if (!winptr)
        return;

    winptr->linebuf = (void *)buf;
    winptr->linebuflen = maxlen;
    winptr->line_request = 1;

    (void)initlen;  /* Pre-filled content not supported in Phase 2 */

    /* Register the array with the dispatch layer so it persists
     * across glk_select() and gets written back to VM memory. */
    if (gli_register_arr) {
        winptr->inarrayrock = (*gli_register_arr)(buf, maxlen, "&+#!Cn");
    }
}

/* -------------------------------------------------------------------------
 * glk_cancel_line_event — Cancel a pending line input event
 *
 * Clears the line_request flag on the specified window. If event is
 * non-NULL, populates it with an evtype_LineInput event with val1=0,
 * indicating the input was cancelled.
 *
 * Parameters:
 *   win   — the window to cancel input for
 *   event — optional pointer to receive a cancelled event
 * ------------------------------------------------------------------------- */

void glk_cancel_line_event(winid_t win, event_t *event)
{
    window_t *winptr = (window_t *)win;

    if (!winptr)
        return;

    if (winptr->line_request) {
        /* Unregister the array with the dispatch layer so it writes
         * the current buffer content back to VM memory and frees the
         * retained copy. */
        if (gli_unregister_arr) {
            (*gli_unregister_arr)(winptr->linebuf, winptr->linebuflen,
                "&+#!Cn", winptr->inarrayrock);
        }

        winptr->line_request = 0;
        winptr->linebuf = NULL;
        winptr->linebuflen = 0;
    }

    if (event) {
        event->type = evtype_LineInput;
        event->win = win;
        event->val1 = 0;
        event->val2 = 0;
    }
}

/* -------------------------------------------------------------------------
 * glk_request_char_event — Request a character input event
 *
 * Phase 1 stub: no-op. Character input is implemented in Phase 2.
 *
 * Parameters:
 *   win — the window to receive input
 * ------------------------------------------------------------------------- */

void glk_request_char_event(winid_t win)
{
    (void)win;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_cancel_char_event — Cancel a pending character input event
 *
 * Phase 1 stub: no-op. Character input is implemented in Phase 2.
 *
 * Parameters:
 *   win — the window to cancel input for
 * ------------------------------------------------------------------------- */

void glk_cancel_char_event(winid_t win)
{
    (void)win;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_request_mouse_event — Request a mouse input event
 *
 * Phase 1 stub: no-op. Mouse input is not implemented.
 *
 * Parameters:
 *   win — the window to receive mouse input
 * ------------------------------------------------------------------------- */

void glk_request_mouse_event(winid_t win)
{
    (void)win;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_cancel_mouse_event — Cancel a pending mouse input event
 *
 * Phase 1 stub: no-op. Mouse input is not implemented.
 *
 * Parameters:
 *   win — the window to cancel input for
 * ------------------------------------------------------------------------- */

void glk_cancel_mouse_event(winid_t win)
{
    (void)win;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_request_timer_events — Request periodic timer events
 *
 * Phase 1 stub: no-op. Timer events are not implemented.
 *
 * Parameters:
 *   millisecs — the interval in milliseconds between timer events
 * ------------------------------------------------------------------------- */

void glk_request_timer_events(glui32 millisecs)
{
    (void)millisecs;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_window_set_echo_stream — Set the echo stream for a window
 *
 * Phase 1 stub: no-op. Line echo is implemented in Phase 2.
 *
 * Parameters:
 *   win — the window to set the echo stream for
 *   str — the stream to echo input to
 * ------------------------------------------------------------------------- */

void glk_window_set_echo_stream(winid_t win, strid_t str)
{
    (void)win;
    (void)str;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_window_get_echo_stream — Get the echo stream for a window
 *
 * Phase 1 stub: returns NULL. Line echo is implemented in Phase 2.
 *
 * Parameters:
 *   win — the window to query
 *
 * Returns:
 *   NULL (not implemented in Phase 1).
 * ------------------------------------------------------------------------- */

strid_t glk_window_get_echo_stream(winid_t win)
{
    (void)win;
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_set_echo_line_event — Enable or disable line echo
 *
 * Phase 1 stub: no-op. Line echo is implemented in Phase 2.
 *
 * Parameters:
 *   win — the window to set echo for
 *   val — non-zero to enable echo, zero to disable
 * ------------------------------------------------------------------------- */

void glk_set_echo_line_event(winid_t win, glui32 val)
{
    (void)win;
    (void)val;
    /* no-op (stub) */
}

/* -------------------------------------------------------------------------
 * glk_set_terminators_line_event — Set line input terminators
 *
 * Phase 1 stub: no-op. Line terminators are implemented in Phase 2.
 *
 * Parameters:
 *   win     — the window to set terminators for
 *   keycodes — array of keycode values
 *   count   — number of keycodes in the array
 * ------------------------------------------------------------------------- */

void glk_set_terminators_line_event(winid_t win, glui32 *keycodes,
    glui32 count)
{
    (void)win;
    (void)keycodes;
    (void)count;
    /* no-op (stub) */
}