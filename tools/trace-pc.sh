#!/bin/bash
set -e
cd "$(dirname "$0")/.."

GAME="$HOME/Projects/lotr-adventure/LOTR.inform/Build/output.ulx"

echo "=== Building CheapGlk ==="
cd thirdparty/cheapglk && make clean > /dev/null 2>&1 && make > /dev/null 2>&1
cd ../..

echo "=== Building Git (CheapGlk) ==="
cd thirdparty/git
make clean > /dev/null 2>&1
make GLK=cheapglk GLKINCLUDEDIR=../cheapglk GLKLIBDIR=../cheapglk OPTIONS="-DTRACE_SAVE_PC" > /dev/null 2>&1
mv git gitc
cd ../..

echo "=== Building NextGlk ==="
cd nextglk && make clean > /dev/null 2>&1 && make > /dev/null 2>&1
cd ..

echo "=== Building Git (NextGlk) ==="
cd thirdparty/git
make clean > /dev/null 2>&1
make GLK=nextglk GLKINCLUDEDIR=../../nextglk GLKLIBDIR=../../nextglk OPTIONS="-DTRACE_SAVE_PC -DUSE_MMAP" > /dev/null 2>&1
mv git gitn
cd ../..

echo "=== CheapGlk Save/Resume PC Trace ==="
cd thirdparty/git
rm -f nextgit.sav
timeout 10 bash -c "printf 'save\nrestore\nlook\nquit\n' | ./gitc \"$GAME\" 2>/tmp/trace-cheap-pc.log" 2>&1 || true
cd ../..
echo "CheapGlk trace lines: $(wc -l < /tmp/trace-cheap-pc.log)"
grep -E "SAVE_PC" /tmp/trace-cheap-pc.log

echo ""
echo "=== NextGlk Save/Resume PC Trace ==="
cd thirdparty/git
rm -f nextgit.sav
timeout 10 bash -c "printf 'save\nrestore\nlook\nquit\n' | ./gitn \"$GAME\" 2>/tmp/trace-next-pc.log" 2>&1 || true
cd ../..
echo "NextGlk trace lines: $(wc -l < /tmp/trace-next-pc.log)"
grep -E "SAVE_PC" /tmp/trace-next-pc.log