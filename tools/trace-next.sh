#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Building NextGlk ==="
cd nextglk
make clean
make
cd ..

echo "=== Building Git (NextGlk) ==="
cd thirdparty/git
make GLK=nextglk GLKINCLUDEDIR=../../nextglk GLKLIBDIR=../../nextglk clean
make GLK=nextglk GLKINCLUDEDIR=../../nextglk GLKLIBDIR=../../nextglk OPTIONS="-DUSE_MMAP"
mv git gitn
cd ../..

echo "=== Running trace ==="
cd thirdparty/git
rm -f nextgit.sav
printf "save\nrestore\nquit\n" | ./gitn "$HOME/Projects/lotr-adventure/LOTR.inform/Build/output.ulx" 2>../../trace-next.log
cd ../..

grep -E "TRACE|req_line_event" trace-next.log | sed 's/0x[0-9a-f]\{6,\}/ADDR/g' > trace-next-filtered.log
echo "Done."