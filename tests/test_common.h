/* test_common.h — Shared test infrastructure
 *
 * Provides TEST_ASSERT macro and external counters.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>

extern int tests_passed;
extern int tests_failed;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
        return 1; \
    } else { \
        printf("  PASS: %s\n", msg); \
        tests_passed++; \
    } \
} while (0)

#endif /* TEST_COMMON_H */