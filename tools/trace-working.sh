#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Running trace: working (look, quit) ==="
cd thirdparty/git
rm -f nextgit.sav
printf "look\nquit\n" | ./gitn "$HOME/Projects/lotr-adventure/LOTR.inform/Build/output.ulx" 2>../../trace-working.log
cd ../..

grep -E "TRACE|req_line_|glk_select|DEBUG" trace-working.log | sed 's/0x[0-9a-f]\{6,\}/ADDR/g' > trace-working-filtered.log
echo "Done. Output: trace-working.log, trace-working-filtered.log"