/* glkstart.c: Unix-specific startup code for NextGlk.
 *
 * Provides the glkunix_* interface required by git_unix.c.
 * See docs/integration-attempt-1.md §3a for the required symbols.
 *
 * This is a minimal implementation:
 *   - glkunix_startup_code() and glkunix_arguments[] are provided by
 *     git_unix.c (the Git VM's startup routine). This file only provides
 *     stubs for glkunix_stream_open_pathname*() and
 *     glkunix_set_base_file().
 *
 * With -DUSE_MMAP, these stubs are never called (the MMAP path opens
 * the game file directly via open()/mmap()), so glkstart.o will not
 * be pulled from the archive at link time.
 */

#include "glk.h"
#include "glkstart.h"

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