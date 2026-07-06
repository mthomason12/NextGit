#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Building CheapGlk ==="
cd thirdparty/cheapglk
make clean
make
cd ../..

echo "=== Building Git (CheapGlk) ==="
cd thirdparty/git
make clean
make
mv git gitc
cd ../..

echo "=== Running trace ==="
cd thirdparty/git
rm -f nextgit.sav
printf "save\nrestore\nquit\n" | ./gitc "$HOME/Projects/lotr-adventure/LOTR.inform/Build/output.ulx" 2>../../trace-cheap.log
cd ../..

grep -E "TRACE|req_line_event" trace-cheap.log | sed 's/0x[0-9a-f]\{6,\}/ADDR/g' > trace-cheap-filtered.log
echo "Done."