/* main.c: Minimal main() for NextGlk.
 *
 * Provides the entry point that calls glkunix_startup_code()
 * and then glk_main(), as expected by the Git VM.
 *
 * This is a minimal implementation — no argument parsing.
 * See docs/integration-attempt-1.md §6 for the expected startup flow.
 */

#include <stdlib.h>
#include "glk.h"
#include "glkstart.h"

int main(int argc, char *argv[])
{
    glkunix_startup_t startdata;

    startdata.argc = argc;
    startdata.argv = argv;

    if (!glkunix_startup_code(&startdata)) {
        glk_exit();
    }

    glk_main();
    glk_exit();

    /* glk_exit() doesn't return, but the compiler may kvetch if main()
     * doesn't seem to return a value. */
    return 0;
}