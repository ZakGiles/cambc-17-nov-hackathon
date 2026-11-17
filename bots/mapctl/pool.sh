#!/bin/bash
# Score margin of BOT on one map against a pool of bots, and the sum. One game each: seats mirror.
#   bots/mapctl/pool.sh MAP [BOT]     MAP: lair, frontier or checkerboard. BOT: default mapctl.
cd "$(dirname "$0")/../.."
BOT=${2:-mapctl}
phais build "$BOT" 2>&1 | grep -E "error" && exit 1
tot=0; out=""
for o in v1 v2 v3 main build-6 starter gen opp-s1 opp-s2; do
  [ -d bots/$o ] || continue
  m=$(python3 tools/arena.py "$BOT" $o --games 2 --maps "maps/$1.json" 2>&1 | grep -E '^ +1  [a-z]' | head -1 | awk '{gsub(",","",$5); print $5+0}')
  out="$out $o:$m"; tot=$((tot + m))
done
echo "$1 sum $tot |$out"
