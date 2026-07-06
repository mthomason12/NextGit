#!/bin/bash
set -e
cd "$(dirname "$0")/.."

ULX="$HOME/Projects/lotr-adventure/LOTR.inform/Build/output.ulx"
SAVEDIR="$HOME/.local/share/cheapglk"

echo "=== Scenario A: start, look, look ==="
echo ""

echo "--- CheapGlk ---"
rm -rf "$SAVEDIR"
printf "look\nlook\nquit\n" | \
  ./thirdparty/git/gitc "$ULX" 2>./bench-scenario-a-cheapglk.log
echo "CheapGlk exit: $?"

echo ""
echo "--- NextGlk ---"
rm -rf "$SAVEDIR"
printf "look\nlook\nquit\n" | \
  ./thirdparty/git/gitn "$ULX" 2>./bench-scenario-a-nextglk.log
echo "NextGlk exit: $?"

echo ""
echo "=== Filtering BENCH_TRACE lines ==="
grep "BENCH_TRACE" bench-scenario-a-cheapglk.log \
  > bench-scenario-a-cheapglk-filtered.log
grep "BENCH_TRACE" bench-scenario-a-nextglk.log \
  > bench-scenario-a-nextglk-filtered.log

echo ""
echo "CheapGlk BENCH_TRACE count: $(wc -l < bench-scenario-a-cheapglk-filtered.log)"
echo "NextGlk BENCH_TRACE count: $(wc -l < bench-scenario-a-nextglk-filtered.log)"

echo ""
echo "=== DIFF ==="
diff bench-scenario-a-cheapglk-filtered.log bench-scenario-a-nextglk-filtered.log \
  || echo "*** DIVERGENCE FOUND ***"