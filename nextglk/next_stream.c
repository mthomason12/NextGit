#include "nextglk_internal.h"

/* -------------------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------------------- */

stream_t *gli_streamlist = NULL;
stream_t *gli_currentstr = NULL;

/* -------------------------------------------------------------------------
 * gli_new_stream — Allocate and initialise a stream
 *
 * Allocates a stream_t, initialises all fields to zero/NULL, sets the
 * caller-provided type/readable/writable/rock, inserts the stream into
 * gli_streamlist (head insertion), and registers it with the dispatch
 * layer if the registration callback is available.
 *
 * Parameters:
 *   type     — strtype_Window, strtype_File, or strtype_Memory
 *   readable — non-zero if the stream supports reading
 *   writable — non-zero if the stream supports writing
 *   rock     — game-supplied rock value (stored but not used yet)
 *
 * Returns:
 *   Pointer to the newly allocated stream, or NULL on allocation failure.
 * ------------------------------------------------------------------------- */

stream_t *gli_new_stream(int type, int readable, int writable, glui32 rock)
{
    stream_t *str;

    (void)rock;  /* rock value is stored for future use */

    str = (stream_t *)malloc(sizeof(stream_t));
    if (!str)
        return NULL;

    /* Zero-initialise all fields */
    str->disprock.num = 0;
    str->type = type;
    str->unicode = 0;
    str->readable = readable;
    str->writable = writable;
    str->readcount = 0;
    str->writecount = 0;
    str->win = NULL;
    str->file = NULL;
    str->isbinary = 0;
    str->buf = NULL;
    str->bufptr = NULL;
    str->bufend = NULL;
    str->bufeof = NULL;
    str->ubuf = NULL;
    str->ubufptr = NULL;
    str->ubufend = NULL;
    str->ubufeof = NULL;
    str->buflen = 0;

    /* Insert at head of gli_streamlist */
    str->prev = NULL;
    str->next = gli_streamlist;
    if (gli_streamlist)
        gli_streamlist->prev = str;
    gli_streamlist = str;

    /* Register with dispatch layer if callback is available */
    if (gli_register_obj)
        str->disprock = (*gli_register_obj)(str, gidisp_Class_Stream);

    return str;
}

/* -------------------------------------------------------------------------
 * gli_stream_fill_result — Copy stream counts into a stream_result_t
 *
 * If result is non-NULL, copies the stream's readcount and writecount
 * into the result struct. If result is NULL, does nothing.
 * ------------------------------------------------------------------------- */

void gli_stream_fill_result(stream_t *str, stream_result_t *result)
{
    if (result) {
        result->readcount = str->readcount;
        result->writecount = str->writecount;
    }
}

/* -------------------------------------------------------------------------
 * gli_delete_stream — Destroy a stream
 *
 * Fills the stream result (if requested), unregisters the stream from the
 * dispatch layer, unlinks the stream from gli_streamlist, and frees the
 * stream structure.
 *
 * Type-specific cleanup:
 *   strtype_File:    fclose(str->file) — deferred until file I/O is implemented
 *   strtype_Memory:  nothing (buffer is not owned)
 *   strtype_Window:  nothing (window is not owned)
 *   strtype_Resource: nothing (buffer is not owned)
 *
 * Parameters:
 *   str    — the stream to destroy (must not be NULL)
 *   result — optional pointer to receive the final readcount/writecount
 * ------------------------------------------------------------------------- */

void gli_delete_stream(stream_t *str, stream_result_t *result)
{
    if (!str)
        return;

    /* Fill result before destroying data */
    gli_stream_fill_result(str, result);

    /* Clear current stream pointer if this is the current stream */
    if (gli_currentstr == str)
        gli_currentstr = NULL;

    /* Unregister from dispatch layer */
    if (gli_unregister_obj)
        (*gli_unregister_obj)(str, gidisp_Class_Stream, str->disprock);

    /* Unlink from gli_streamlist */
    if (str->prev)
        str->prev->next = str->next;
    else
        gli_streamlist = str->next;

    if (str->next)
        str->next->prev = str->prev;

    /* Free the stream */
    free(str);
}

/* -------------------------------------------------------------------------
 * glk_stream_iterate — Iterate over all active streams
 *
 * If str is NULL, returns the first stream in gli_streamlist.
 * Otherwise, returns str->next (the next stream in the list).
 * If rockptr is non-NULL, writes the stream's rock value into *rockptr.
 *
 * Returns NULL when no more streams exist.
 * ------------------------------------------------------------------------- */

strid_t glk_stream_iterate(strid_t str, glui32 *rockptr)
{
    stream_t *s = (stream_t *)str;

    if (s == NULL)
        s = gli_streamlist;
    else
        s = s->next;

    if (s) {
        if (rockptr)
            *rockptr = 0;  /* rock value — currently always 0 */
        return s;
    }
    else {
        if (rockptr)
            *rockptr = 0;
        return NULL;
    }
}

/* -------------------------------------------------------------------------
 * glk_stream_get_rock — Return the rock value of a stream
 *
 * Currently a stub — always returns 0.
 * Required by the dispatch function table.
 * ------------------------------------------------------------------------- */

glui32 glk_stream_get_rock(strid_t str)
{
    (void)str;  /* stub — rock value not yet implemented */
    return 0;
}
