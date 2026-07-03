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
 * Phase 3A.2 — Fileref creation functions:
 *   - glk_fileref_create_by_name()
 *   - glk_fileref_create_temp()
 *   - glk_fileref_create_by_prompt()
 *   - glk_fileref_create_from_fileref()
 *
 * Phase 3A.7 stubs (not implemented yet):
 *   - glk_fileref_delete_file
 *   - glk_fileref_does_file_exist
 */

#define _POSIX_C_SOURCE 200809L

#include "glk.h"
#include "nextglk_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------------------- */

fileref_t *gli_filereflist = NULL;

/* -------------------------------------------------------------------------
 * Internal helpers for fileref creation (Phase 3A.2)
 * ------------------------------------------------------------------------- */

/*
 * has_known_extension — Check whether a filename already ends with a
 * recognised Glk file extension.
 *
 * Recognised extensions: .glkdata, .glksave, .txt, .glkrec
 *
 * Returns 1 if the name has a recognised extension, 0 otherwise.
 */
static int has_known_extension(const char *name)
{
    size_t namelen;
    const char *known[] = { ".glkdata", ".glksave", ".txt", ".glkrec", NULL };
    int i;

    if (!name)
        return 0;

    namelen = strlen(name);

    for (i = 0; known[i] != NULL; i++) {
        size_t extlen = strlen(known[i]);

        if (namelen >= extlen &&
            strcmp(name + namelen - extlen, known[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

/*
 * suffix_for_usage — Return the file extension suffix for a given
 * fileusage_* type.
 *
 * The suffix is determined by (usage & fileusage_TypeMask):
 *   fileusage_Data        -> ".glkdata"
 *   fileusage_SavedGame   -> ".glksave"
 *   fileusage_Transcript  -> ".txt"
 *   fileusage_InputRecord -> ".glkrec"
 *
 * Returns NULL for unknown usage types.
 */
static const char *suffix_for_usage(glui32 usage)
{
    switch (usage & fileusage_TypeMask) {
    case fileusage_Data:
        return ".glkdata";
    case fileusage_SavedGame:
        return ".glksave";
    case fileusage_Transcript:
        return ".txt";
    case fileusage_InputRecord:
        return ".glkrec";
    default:
        return NULL;
    }
}

/*
 * fixed_name_for_prompt — Return the fixed filename used by
 * glk_fileref_create_by_prompt for a given usage.
 *
 *   fileusage_SavedGame   -> "nextgit.sav"
 *   fileusage_Transcript  -> "transcript.txt"
 *   fileusage_InputRecord -> "input.rec"
 *   fileusage_Data        -> "data.glkdata"
 *
 * Returns NULL for unknown usage types.
 */
static const char *fixed_name_for_prompt(glui32 usage)
{
    switch (usage & fileusage_TypeMask) {
    case fileusage_SavedGame:
        return "nextgit.sav";
    case fileusage_Transcript:
        return "transcript.txt";
    case fileusage_InputRecord:
        return "input.rec";
    case fileusage_Data:
        return "data.glkdata";
    default:
        return NULL;
    }
}

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
 * Phase 3A.2 — Fileref Creation Functions
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * glk_fileref_create_by_name — Create a fileref from a game-supplied name
 *
 * Appends an appropriate suffix based on the usage type if the name does
 * not already have a recognised Glk extension.  The suffix is determined
 * by (usage & fileusage_TypeMask):
 *
 *   fileusage_Data        -> .glkdata
 *   fileusage_SavedGame   -> .glksave
 *   fileusage_Transcript  -> .txt
 *   fileusage_InputRecord -> .glkrec
 *
 * Does NOT create the actual file — only the fileref.
 *
 * Returns the new fileref via gli_new_fileref(), or NULL on failure.
 * ------------------------------------------------------------------------- */

frefid_t glk_fileref_create_by_name(glui32 usage, char *name, glui32 rock)
{
    const char *suffix;
    char *fullname;
    size_t namelen, suffixlen;
    fileref_t *fref;

    if (!name)
        return NULL;

    suffix = suffix_for_usage(usage);
    if (!suffix)
        return NULL;

    /*
     * If the name already has a recognised Glk extension, use it as-is.
     * Otherwise, append the suffix determined by the usage type.
     */
    if (has_known_extension(name)) {
        fullname = strdup(name);
    } else {
        namelen = strlen(name);
        suffixlen = strlen(suffix);
        fullname = (char *)malloc(namelen + suffixlen + 1);
        if (!fullname)
            return NULL;

        memcpy(fullname, name, namelen);
        memcpy(fullname + namelen, suffix, suffixlen + 1); /* include NUL */
    }

    if (!fullname)
        return NULL;

    fref = gli_new_fileref(fullname, usage, rock);
    free(fullname);
    return fref;
}

/* -------------------------------------------------------------------------
 * glk_fileref_create_temp — Create a fileref for a temporary file
 *
 * Generates a unique temporary filename using mkstemp(), which also
 * creates the empty file.  The file descriptor is closed immediately;
 * the file remains on disk as an empty placeholder.
 *
 * The template used is "nextgit-XXXXXX" in the current working directory.
 *
 * Returns the new fileref via gli_new_fileref(), or NULL on failure.
 * ------------------------------------------------------------------------- */

frefid_t glk_fileref_create_temp(glui32 usage, glui32 rock)
{
    char *tempname;
    int fd;
    fileref_t *fref;

    tempname = strdup("nextgit-XXXXXX");
    if (!tempname)
        return NULL;

    fd = mkstemp(tempname);
    if (fd < 0) {
        free(tempname);
        return NULL;
    }

    /* Close the fd immediately — the file exists but is empty */
    close(fd);

    fref = gli_new_fileref(tempname, usage, rock);
    free(tempname);
    return fref;
}

/* -------------------------------------------------------------------------
 * glk_fileref_create_by_prompt — Create a fileref using a fixed filename
 *
 * On the ZX Spectrum Next there is no interactive prompt.  This function
 * returns a fileref with a predetermined filename based on usage:
 *
 *   fileusage_SavedGame   -> "nextgit.sav"
 *   fileusage_Transcript  -> "transcript.txt"
 *   fileusage_InputRecord -> "input.rec"
 *   fileusage_Data        -> "data.glkdata"
 *
 * The fmode parameter is accepted but ignored in this implementation.
 *
 * Returns the new fileref via gli_new_fileref(), or NULL on failure.
 * ------------------------------------------------------------------------- */

frefid_t glk_fileref_create_by_prompt(glui32 usage, glui32 fmode, glui32 rock)
{
    const char *fixed_name;

    (void)fmode; /* accepted but ignored — no interactive prompt */

    fixed_name = fixed_name_for_prompt(usage);
    if (!fixed_name)
        return NULL;

    return gli_new_fileref((char *)fixed_name, usage, rock);
}

/* -------------------------------------------------------------------------
 * glk_fileref_create_from_fileref — Clone an existing fileref
 *
 * Creates a new fileref with the same filename as the source fileref but
 * with the supplied usage and rock values.
 *
 * Returns the new fileref via gli_new_fileref(), or NULL on failure.
 * ------------------------------------------------------------------------- */

frefid_t glk_fileref_create_from_fileref(glui32 usage, frefid_t fref,
    glui32 rock)
{
    fileref_t *src = (fileref_t *)fref;

    if (!src || !src->filename)
        return NULL;

    return gli_new_fileref(src->filename, usage, rock);
}

/* =========================================================================
 * Phase 3A.7 Stubs — Fileref Query Functions (not implemented yet)
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * glk_fileref_delete_file — Delete the file referenced by a fileref
 * ------------------------------------------------------------------------- */
void glk_fileref_delete_file(frefid_t fref)
{
    (void)fref;

    /* Stub — no-op.  Implemented in Phase 3A.7. */
}

/* -------------------------------------------------------------------------
 * glk_fileref_does_file_exist — Check if the referenced file exists
 * ------------------------------------------------------------------------- */
glui32 glk_fileref_does_file_exist(frefid_t fref)
{
    (void)fref;

    /* Stub — returns 0 (false).  Implemented in Phase 3A.7. */
    return 0;
}