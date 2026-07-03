/* test_main.c — Test runner for NextGlk Commit 5 (Convenience output + stubs)
 *
 * Runs stream, window, dispatch, stream output, and glk output lifecycle tests,
 * prints PASS/FAIL summary.
 * Returns 0 if all tests pass, non-zero on any failure.
 */

#include "test_common.h"
#include <stdio.h>

/* Test suites declared in their respective .c files */
extern int stream_tests_run(void);
extern int window_tests_run(void);
extern int dispatch_tests_run(void);
extern int stream_output_tests_run(void);
extern int glk_output_tests_run(void);
extern int input_tests_run(void);

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
    tests_passed = 0;
    tests_failed = 0;

    printf("\n=== Dispatch Registration Tests ===\n");
    result = dispatch_tests_run();
    if (result != 0) {
        printf("DISPATCH TESTS FAILED\n");
        return 1;
    }
    printf("Dispatch tests: %d passed, %d failed\n", tests_passed, tests_failed);
    tests_passed = 0;
    tests_failed = 0;

    printf("\n=== Stream Output Tests ===\n");
    result = stream_output_tests_run();
    if (result != 0) {
        printf("STREAM OUTPUT TESTS FAILED\n");
        return 1;
    }
    printf("Stream output tests: %d passed, %d failed\n", tests_passed, tests_failed);
    tests_passed = 0;
    tests_failed = 0;

    printf("\n=== Glk Output Tests (Commit 5) ===\n");
    result = glk_output_tests_run();
    if (result != 0) {
        printf("GLK OUTPUT TESTS FAILED\n");
        return 1;
    }
    printf("Glk output tests: %d passed, %d failed\n", tests_passed, tests_failed);
    tests_passed = 0;
    tests_failed = 0;

    if (tests_failed > 0) {
        printf("\n*** SOME TESTS FAILED ***\n");
        return 1;
    }

    printf("\n=== Input System Tests (Runtime Attempt 2) ===\n");
    result = input_tests_run();
    if (result != 0) {
        printf("INPUT TESTS FAILED\n");
        return 1;
    }
    printf("Input tests: %d passed, %d failed\n", tests_passed, tests_failed);
    tests_passed = 0;
    tests_failed = 0;

    if (tests_failed > 0) {
        printf("\n*** SOME TESTS FAILED ***\n");
        return 1;
    }

    printf("\n*** ALL TESTS PASSED ***\n");
    return 0;
}
