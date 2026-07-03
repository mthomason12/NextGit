/*
 * nextglk_fileref.c — Fileref stubs for NextGit.
 *
 * Implements all glk_fileref_* functions as compileable stubs.
 * No real file system access is performed.
 *
 * See docs/integration-attempt-1.md §3c for the required symbols.
 */

#include "glk.h"
#include "nextglk_internal.h"

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
 * glk_fileref_destroy — Destroy a fileref
 * ------------------------------------------------------------------------- */
void glk_fileref_destroy(frefid_t fref)
{
    (void)fref;

    /* Stub — no-op. */
}

/* -------------------------------------------------------------------------
 * glk_fileref_iterate — Iterate over existing filerefs
 * ------------------------------------------------------------------------- */
frefid_t glk_fileref_iterate(frefid_t fref, glui32 *rockptr)
{
    (void)fref;
    (void)rockptr;

    /* Stub — returns NULL (no filerefs exist). */
    return NULL;
}

/* -------------------------------------------------------------------------
 * glk_fileref_get_rock — Get the rock value of a fileref
 * ------------------------------------------------------------------------- */
glui32 glk_fileref_get_rock(frefid_t fref)
{
    (void)fref;

    /* Stub — returns 0. */
    return 0;
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