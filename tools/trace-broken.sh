#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Running trace: broken (save, look, quit) ==="
cd thirdparty/git
rm -f nextgit.sav
printf "save\nnextgit.sav\nlook\nquit\n" | ./gitn "$HOME/Projects/lotr-adventure/LOTR.inform/Build/output.ulx" 2>../../trace-broken.log
cd ../..

grep -E "TRACE|req_line_|glk_select|DEBUG" trace-broken.log | sed 's/0x[0-9a-f]\{6,\}/ADDR/g' > trace-broken-filtered.log
echo "Done. Output: trace-broken.log, trace-broken-filtered.log"