#ifndef NEXTGLK_INTERNAL_H
#define NEXTGLK_INTERNAL_H

#include "glk.h"
#include "gi_dispa.h"

#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Stream type constants (not defined in glk.h)
 * ------------------------------------------------------------------------- */

#define strtype_Window  0
#define strtype_File    1
#define strtype_Memory  2
#define strtype_Resource 3

/* -------------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------------- */

typedef struct glk_window_struct  window_t;
typedef struct glk_fileref_struct fileref_t;

/* -------------------------------------------------------------------------
 * stream_t — Stream object
 *
 * Defined here because windows depend on streams.
 * Ownership rules (from docs/glk-object-lifecycle.md):
 *   - win:       NOT OWNED (back-pointer to owning window)
 *   - file:      OWNED (fclose()'d on stream destruction)
 *   - buf/ubuf:  NOT OWNED (caller-provided buffer, not freed)
 * ------------------------------------------------------------------------- */

typedef struct glk_stream_struct stream_t;
struct glk_stream_struct {
    /* Dispatch-layer bookkeeping (MANDATORY) */
    gidispatch_rock_t disprock;

    /* Type and mode */
    int type;                   /* strtype_Window / strtype_File / strtype_Memory */
    int unicode;                /* 0 = 1-byte chars, 1 = 4-byte */
    int readable, writable;
    glui32 rock;                /* game-supplied rock value */
    glui32 readcount, writecount;   /* for stream_result_t */

    /* For strtype_Window */
    window_t *win;              /* back-pointer to owning window (NOT OWNED) */

    /* For strtype_File */
    void *file;                 /* platform file handle (deferred) */

    /* For strtype_Memory and strtype_Resource */
    int isbinary;
    unsigned char *buf, *bufptr, *bufend, *bufeof;
    glui32 *ubuf, *ubufptr, *ubufend, *ubufeof;
    glui32 buflen;

    /* Linked list (MANDATORY for iteration) */
    struct glk_stream_struct *next, *prev;
};

/* -------------------------------------------------------------------------
 * window_t — Window object
 *
 * Ownership rules (from docs/glk-object-lifecycle.md):
 *   - str:       OWNED (created by gli_new_window(), destroyed by gli_delete_window())
 *   - echostr:   NOT OWNED (set by game, not closed on window close)
 *   - linebuf:   NOT OWNED (points into VM memory, not freed)
 * ------------------------------------------------------------------------- */

struct glk_window_struct {
    /* Dispatch-layer bookkeeping (MANDATORY) */
    gidispatch_rock_t disprock;

    /* Streams */
    stream_t *str;              /* window's output stream (OWNED) */
    stream_t *echostr;          /* echo stream, or NULL (NOT OWNED) */

    /* Input state */
    int line_request;           /* 1 if line input is pending */
    int line_request_uni;       /* 1 if line input is Unicode */
    int char_request;           /* 1 if char input is pending */
    int char_request_uni;       /* 1 if char input is Unicode */

    /* Line buffer (points to VM memory, NOT OWNED) */
    void *linebuf;
    glui32 linebuflen;
    gidispatch_rock_t inarrayrock;  /* dispatch-layer array registration */
};

/* -------------------------------------------------------------------------
 * fileref_t — Fileref object
 *
 * Ownership rules (from docs/glk-object-lifecycle.md):
 *   - filename: OWNED (malloc'd by gli_new_fileref, freed by gli_delete_fileref)
 *
 * Filerefs are tracked in gli_filereflist (doubly-linked list, head insertion).
 * Dispatch-layer registration happens in both creation and destruction paths.
 * ------------------------------------------------------------------------- */

struct glk_fileref_struct {
    /* Dispatch-layer bookkeeping (MANDATORY) */
    gidispatch_rock_t disprock;

    /* Game-supplied rock value */
    glui32 rock;

    /* File usage mask (fileusage_Data, fileusage_SavedGame, etc.) */
    glui32 usage;

    /* Owned filename string (malloc'd) */
    char *filename;

    /* Linked list (MANDATORY for iteration) */
    struct glk_fileref_struct *next, *prev;
};

/* -------------------------------------------------------------------------
 * Global state
 *
 * Defined in next_stream.c, next_window.c, or nextglk_fileref.c,
 * declared extern here.
 * ------------------------------------------------------------------------- */

extern stream_t *gli_streamlist;
extern stream_t *gli_currentstr;
extern window_t *gli_mainwin;
extern fileref_t *gli_filereflist;

/* -------------------------------------------------------------------------
 * Dispatch-layer registration callbacks
 *
 * These function pointers are set by the Git VM via
 * gidispatch_set_object_registry() and gidispatch_set_retained_registry()
 * during startup.  They may be NULL before the VM initialises the
 * dispatch layer.
 * ------------------------------------------------------------------------- */

extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass,
    gidispatch_rock_t objrock);

extern gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len,
    char *typecode);
extern void (*gli_unregister_arr)(void *array, glui32 len, char *typecode,
    gidispatch_rock_t objrock);

/* -------------------------------------------------------------------------
 * Stream lifecycle functions
 * ------------------------------------------------------------------------- */

stream_t *gli_new_stream(int type, int readable, int writable, glui32 rock);
void gli_delete_stream(stream_t *str, stream_result_t *result);
void gli_stream_fill_result(stream_t *str, stream_result_t *result);

/* -------------------------------------------------------------------------
 * Window lifecycle functions
 * ------------------------------------------------------------------------- */

window_t *gli_new_window(glui32 rock);
void gli_delete_window(window_t *win, stream_result_t *result);

/* -------------------------------------------------------------------------
 * Fileref lifecycle functions
 * ------------------------------------------------------------------------- */

fileref_t *gli_new_fileref(char *filename, glui32 usage, glui32 rock);
void gli_delete_fileref(fileref_t *fref);

/* -------------------------------------------------------------------------
 * Window API functions (declared in glk.h, implemented in next_window.c)
 * ------------------------------------------------------------------------- */

winid_t glk_window_open(winid_t split, glui32 method, glui32 size,
    glui32 wintype, glui32 rock);
void glk_window_close(winid_t win, stream_result_t *result);
winid_t glk_window_get_root(void);
glui32 glk_window_get_type(winid_t win);
void glk_window_get_size(winid_t win, glui32 *widthptr, glui32 *heightptr);
strid_t glk_window_get_stream(winid_t win);
winid_t glk_window_get_sibling(winid_t win);
winid_t glk_window_iterate(winid_t win, glui32 *rockptr);
void glk_window_clear(winid_t win);
void glk_set_window(winid_t win);
glui32 glk_window_get_rock(winid_t win);
winid_t glk_window_get_parent(winid_t win);
void glk_window_move_cursor(winid_t win, glui32 xpos, glui32 ypos);

#endif /* NEXTGLK_INTERNAL_H */