#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Building CheapGlk ==="
cd thirdparty/cheapglk
make clean > /dev/null && make > /dev/null
cd ../..

echo "=== Building API test (CheapGlk) ==="
cc -g -Wall \
  -Ithirdparty/cheapglk \
  -Ithirdparty/git \
  tests/glk_api_tests.c \
  -Lthirdparty/cheapglk \
  -lcheapglk \
  -o tests/glk_api_test_cheapglk

echo "=== Running test ==="
LD_LIBRARY_PATH=thirdparty/cheapglk tests/glk_api_test_cheapglk
echo "Exit: $?"