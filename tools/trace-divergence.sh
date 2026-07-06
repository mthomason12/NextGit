#!/bin/bash
set -e
cd "$(dirname "$0")/.."

GAME="$HOME/Projects/lotr-adventure/LOTR.inform/Build/output.ulx"

echo "=== Building CheapGlk (baseline) ==="
cd thirdparty/cheapglk
make clean > /dev/null && make > /dev/null
cd ../..

echo "=== Building Git (CheapGlk) with divergence logging ==="
cd thirdparty/git
make clean > /dev/null
make GLK=cheapglk GLKINCLUDEDIR=../cheapglk GLKLIBDIR=../cheapglk OPTIONS="-DTRACE_DIVERGENCE" > /dev/null
mv git gitc
cd ../..

echo "=== Building NextGlk ==="
cd nextglk
make clean > /dev/null && make > /dev/null
cd ..

echo "=== Building Git (NextGlk) with divergence logging ==="
cd thirdparty/git
make clean > /dev/null
make GLK=nextglk GLKINCLUDEDIR=../../nextglk GLKLIBDIR=../../nextglk OPTIONS="-DTRACE_DIVERGENCE" > /dev/null
mv git gitn
cd ../..

echo "=== Running CheapGlk trace ==="
cd thirdparty/git
rm -f nextgit.sav
timeout 10 bash -c "printf 'save\nrestore\nlook\nquit\n' | ./gitc \"$GAME\" 2>/tmp/trace-cheap-divergence.log" || true
cd ../..

echo "=== Running NextGlk trace ==="
cd thirdparty/git
rm -f nextgit.sav
timeout 10 bash -c "printf 'save\nrestore\nlook\nquit\n' | ./gitn \"$GAME\" 2>/tmp/trace-next-divergence.log" || true
cd ../..

echo "=== CheapGlk trace ==="
echo "--- Request line event calls ---"
grep -E "TRACE_DIV.*request_line|TRACE_DIV.*do_glk.*0x00D0|TRACE_DIV.*save_stub|TRACE_DIV.*restore" /tmp/trace-cheap-divergence.log

echo ""
echo "=== NextGlk trace ==="
echo "--- Request line event calls ---"
grep -E "TRACE_DIV.*request_line|TRACE_DIV.*do_glk.*0x00D0|TRACE_DIV.*save_stub|TRACE_DIV.*restore" /tmp/trace-next-divergence.log