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
 * Global state
 *
 * Defined in next_stream.c, declared extern here.
 * ------------------------------------------------------------------------- */

extern stream_t *gli_streamlist;
extern stream_t *gli_currentstr;
extern window_t *gli_mainwin;
extern fileref_t *gli_filereflist;

/* -------------------------------------------------------------------------
 * Dispatch-layer registration callbacks
 *
 * These function pointers are set by the Git VM via
 * gidispatch_set_object_registry() during startup.
 * They may be NULL before the VM initialises the dispatch layer.
 * ------------------------------------------------------------------------- */

extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass,
    gidispatch_rock_t objrock);

/* -------------------------------------------------------------------------
 * Stream lifecycle functions
 * ------------------------------------------------------------------------- */

stream_t *gli_new_stream(int type, int readable, int writable, glui32 rock);
void gli_delete_stream(stream_t *str, stream_result_t *result);
void gli_stream_fill_result(stream_t *str, stream_result_t *result);

#endif /* NEXTGLK_INTERNAL_H */