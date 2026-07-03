/* test_stream_main.c — Test runner for stream lifecycle tests only */

#include "test_common.h"
#include <stdio.h>

extern int stream_tests_run(void);

int tests_passed = 0;
int tests_failed = 0;

int main(void)
{
    printf("=== Stream Lifecycle Tests ===\n");
    if (stream_tests_run() != 0) {
        printf("\n*** STREAM TESTS FAILED ***\n");
        return 1;
    }
    printf("\n*** ALL STREAM TESTS PASSED ***\n");
    return 0;
}