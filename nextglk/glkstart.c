/* glkstart.c: Unix-specific startup code for NextGlk.
 *
 * Provides the glkunix_* interface required by git_unix.c.
 * See docs/integration-attempt-1.md §3a for the required symbols.
 *
 * This is a minimal implementation:
 *   - glkunix_startup_code() initialises NextGlk and returns success.
 *   - glkunix_stream_open_pathname*() return NULL stubs.
 *   - glkunix_set_base_file() is a stub.
 */

#include "glk.h"
#include "glkstart.h"
#include "nextglk.h"

/* No command-line arguments defined. */
glkunix_argumentlist_t glkunix_arguments[] = {
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    (void)data;

    /* Initialise NextGlk library. */
    if (!nextglk_init())
        return FALSE;

    return TRUE;
}

void glkunix_set_base_file(char *filename)
{
    (void)filename;
    /* Stub — no base file tracking. */
}

strid_t glkunix_stream_open_pathname_gen(char *pathname,
    glui32 writemode, glui32 textmode, glui32 rock)
{
    (void)pathname;
    (void)writemode;
    (void)textmode;
    (void)rock;

    /* Stub — returns NULL. Git uses MMAP path, so this is not called. */
    return NULL;
}

strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode,
    glui32 rock)
{
    (void)pathname;
    (void)textmode;
    (void)rock;

    /* Stub — returns NULL. Git uses MMAP path, so this is not called. */
    return NULL;
}