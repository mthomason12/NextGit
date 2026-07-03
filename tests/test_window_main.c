/* test_window_main.c — Test runner for window lifecycle tests only */

#include "test_common.h"
#include <stdio.h>

extern int window_tests_run(void);

int tests_passed = 0;
int tests_failed = 0;

int main(void)
{
    printf("=== Window Lifecycle Tests ===\n");
    if (window_tests_run() != 0) {
        printf("\n*** WINDOW TESTS FAILED ***\n");
        return 1;
    }
    printf("\n*** ALL WINDOW TESTS PASSED ***\n");
    return 0;
}