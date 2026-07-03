/*
 * nextglk_fileref.c — Fileref lifecycle and query functions.
 *
 * Implements the fileref_t struct lifecycle and the Glk fileref API
 * as defined in docs/phase3-fileio-plan.md.
 *
 * Phase 3A.1 — Fileref struct and lifecycle:
 *   - fileref_t struct (defined in nextglk_internal.h)
 *   - gli_filereflist (doubly-linked list, head insertion)
 *   - gli_new_fileref()
 *   - gli_delete_fileref()
 *   - glk_fileref_iterate()
 *   - glk_fileref_get_rock()
 *   - glk_fileref_destroy()
 *
 * Phase 3A.2 stubs (not implemented yet):
 *   - glk_fileref_create_by_name
 *   - glk_fileref_create_by_prompt
 *   - glk_fileref_create_temp
 *   - glk_fileref_create_from_fileref
 *   - glk_fileref_delete_file
 *   - glk_fileref_does_file_exist
 */

#define _POSIX_C_SOURCE 200809L

#include "glk.h"
#include "nextglk_internal.h"

/* -------------------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------------------- */

fileref_t *gli_filereflist = NULL;

/* =========================================================================
 * Phase 3A.1 — Fileref Lifecycle Functions
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * gli_new_fileref — Allocate and initialise a fileref
 *
 * Allocates a fileref_t, duplicates the filename (ownership transfer),
 * stores usage and rock, inserts the fileref at the head of
 * gli_filereflist, and registers it with the dispatch layer if the
 * registration callback is available.
 *
 * Parameters:
 *   filename — the file name to duplicate (must not be NULL)
 *   usage    — fileusage_* mask
 *   rock     — game-supplied rock value
 *
 * Returns:
 *   Pointer to the newly allocated fileref, or NULL on allocation failure.
 * ------------------------------------------------------------------------- */

fileref_t *gli_new_fileref(char *filename, glui32 usage, glui32 rock)
{
    fileref_t *fref;

    if (!filename)
        return NULL;

    fref = (fileref_t *)malloc(sizeof(fileref_t));
    if (!fref)
        return NULL;

    /* Zero-initialise all fields */
    fref->disprock.num = 0;
    fref->rock = rock;
    fref->usage = usage;
    fref->filename = NULL;
    fref->next = NULL;
    fref->prev = NULL;

    /* Duplicate the filename (ownership transfer) */
    fref->filename = strdup(filename);
    if (!fref->filename) {
        free(fref);
        return NULL;
    }

    /* Insert at head of gli_filereflist */
    fref->prev = NULL;
    fref->next = gli_filereflist;
    if (gli_filereflist)
        gli_filereflist->prev = fref;
    gli_filereflist = fref;

    /* Register with dispatch layer if callback is available */
    if (gli_register_obj)
        fref->disprock = (*gli_register_obj)(fref, gidisp_Class_Fileref);

    return fref;
}

/* -------------------------------------------------------------------------
 * gli_delete_fileref — Destroy a fileref
 *
 * Unregisters the fileref from the dispatch layer, unlinks it from
 * gli_filereflist, frees the owned filename string, and frees the
 * fileref structure itself.
 *
 * Parameters:
 *   fref — the fileref to destroy (must not be NULL)
 * ------------------------------------------------------------------------- */

void gli_delete_fileref(fileref_t *fref)
{
    if (!fref)
        return;

    /* Unregister from dispatch layer */
    if (gli_unregister_obj)
        (*gli_unregister_obj)(fref, gidisp_Class_Fileref, fref->disprock);

    /* Unlink from gli_filereflist */
    if (fref->prev)
        fref->prev->next = fref->next;
    else
        gli_filereflist = fref->next;

    if (fref->next)
        fref->next->prev = fref->prev;

    /* Free owned filename */
    if (fref->filename)
        free(fref->filename);

    /* Free the fileref */
    free(fref);
}

/* =========================================================================
 * Phase 3A.1 — Fileref API Functions (real implementations)
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * glk_fileref_iterate — Iterate over all active filerefs
 *
 * If fref is NULL, returns the first fileref in gli_filereflist.
 * Otherwise, returns fref->next (the next fileref in the list).
 * If rockptr is non-NULL, writes the fileref's rock value into *rockptr.
 *
 * Returns NULL when no more filerefs exist.
 * ------------------------------------------------------------------------- */

frefid_t glk_fileref_iterate(frefid_t fref, glui32 *rockptr)
{
    fileref_t *f = (fileref_t *)fref;

    if (f == NULL)
        f = gli_filereflist;
    else
        f = f->next;

    if (f) {
        if (rockptr)
            *rockptr = f->rock;
        return f;
    } else {
        if (rockptr)
            *rockptr = 0;
        return NULL;
    }
}

/* -------------------------------------------------------------------------
 * glk_fileref_get_rock — Return the rock value of a fileref
 * ------------------------------------------------------------------------- */

glui32 glk_fileref_get_rock(frefid_t fref)
{
    fileref_t *f = (fileref_t *)fref;

    if (!f)
        return 0;

    return f->rock;
}

/* -------------------------------------------------------------------------
 * glk_fileref_destroy — Destroy a fileref
 *
 * Public-facing destroy. Delegates to gli_delete_fileref().
 * ------------------------------------------------------------------------- */

void glk_fileref_destroy(frefid_t fref)
{
    fileref_t *f = (fileref_t *)fref;

    if (!f)
        return;

    gli_delete_fileref(f);
}

/* =========================================================================
 * Phase 3A.2 Stubs — Fileref Creation Functions (not implemented yet)
 *
 * These remain as stubs until Milestone 3A.2.
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * glk_fileref_create_temp — Create a temporary fileref
 * ------------------------------------------------------------------------- */
frefid_t glk_fileref_create_temp(glui32 usage, glui32 rock)
{
    (void)usage;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_fileref_create_by_name — Create a fileref by file name
 * ------------------------------------------------------------------------- */
frefid_t glk_fileref_create_by_name(glui32 usage, char *name, glui32 rock)
{
    (void)usage;
    (void)name;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_fileref_create_by_prompt — Create a fileref by prompting the user
 * ------------------------------------------------------------------------- */
frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock)
{
    (void)usage;
    (void)fmode;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_fileref_create_from_fileref — Create a fileref from another fileref
 * ------------------------------------------------------------------------- */
frefid_t glk_fileref_create_from_fileref(glui32 usage, frefid_t fref,
    glui32 rock)
{
    (void)usage;
    (void)fref;
    (void)rock;

    /* Stub — returns NULL. */
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_fileref_delete_file — Delete the file referenced by a fileref
 * ------------------------------------------------------------------------- */
void glk_fileref_delete_file(frefid_t fref)
{
    (void)fref;

    /* Stub — no-op. */
}

/* -------------------------------------------------------------------------
 * glk_fileref_does_file_exist — Check if the referenced file exists
 * ------------------------------------------------------------------------- */
glui32 glk_fileref_does_file_exist(frefid_t fref)
{
    (void)fref;

    /* Stub — returns 0 (false). */
    return 0;
}