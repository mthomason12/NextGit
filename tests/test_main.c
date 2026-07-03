/* test_main.c — Test runner for NextGlk Commit 2 (Window + Stream lifecycle)
 *
 * Runs stream and window lifecycle tests, prints PASS/FAIL summary.
 * Returns 0 if all tests pass, non-zero on any failure.
 */

#include "test_common.h"
#include <stdio.h>

/* Test suites declared in their respective .c files */
extern int stream_tests_run(void);
extern int window_tests_run(void);

int tests_passed = 0;
int tests_failed = 0;

int main(void)
{
    int result;

    printf("=== Stream Lifecycle Tests ===\n");
    result = stream_tests_run();
    if (result != 0) {
        printf("STREAM TESTS FAILED\n");
        return 1;
    }
    printf("Stream tests: %d passed, %d failed\n", tests_passed, tests_failed);
    tests_passed = 0;
    tests_failed = 0;

    printf("\n=== Window Lifecycle Tests ===\n");
    result = window_tests_run();
    if (result != 0) {
        printf("WINDOW TESTS FAILED\n");
        return 1;
    }
    printf("Window tests: %d passed, %d failed\n", tests_passed, tests_failed);

    if (tests_failed > 0) {
        printf("\n*** SOME TESTS FAILED ***\n");
        return 1;
    }

    printf("\n*** ALL TESTS PASSED ***\n");
    return 0;
}