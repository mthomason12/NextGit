#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Building NextGlk ==="
cd nextglk
make clean > /dev/null && make > /dev/null
cd ..

echo "=== Building API test (NextGlk) ==="
cc -g -Wall \
  -Inextglk \
  -Ithirdparty/git \
  tests/glk_api_tests.c \
  -Lnextglk \
  -lnextglk \
  -o tests/glk_api_test_nextglk

echo "=== Running test ==="
LD_LIBRARY_PATH=nextglk tests/glk_api_test_nextglk 2>&1
echo "Exit: $?"
