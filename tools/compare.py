#!/usr/bin/env python3
"""Compare one bot against every other bot in bots/. Run it before a push or a submit.

  tools/compare.py main                 # main against all the others
  tools/compare.py main s6 v10 starter  # main against these only
  tools/compare.py main --base v10      # also show the change from v10's results

One game a map, as the first seat. Games are deterministic and both seats give
the same score, so one seat is enough. A row is one opponent: the score
difference (ours - theirs) on each map, and the row's sum. Fuel cut-offs of
ours are flagged: any cut-off is a bug.

Read the rows, not only the sum. A bot can beat its parent head to head and
still be worse against the rest.
"""
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = ["checkerboard", "frontier", "lair"]
SKIP = ("build-", "sub", "x")  # submit copies and scratch bots


def build(bot):
    return subprocess.run(["phais", "build", bot], cwd=ROOT, capture_output=True, text=True).returncode == 0


def play(a, b, out):
    shutil.rmtree(out, ignore_errors=True)
    done = subprocess.run(["phais", "run", a, b, "--maps", "maps/", "--seed", "1", "-o", out],
                          cwd=ROOT, capture_output=True, text=True)
    res = {}
    for path in glob.glob(os.path.join(out, "*-map-*.json")):
        d = json.load(open(path))
        s = d["result"]["scores"]
        res[d["map"]["id"]] = (s[0] - s[1], d["fuel_exhausted"][0])
    return res if done.returncode == 0 and len(res) == len(MAPS) else None


def results(bot, others, out):
    rows = {}
    for o in others:
        if not build(o):
            rows[o] = None
            continue
        rows[o] = play(bot, o, out)
    return rows


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    base = sys.argv[sys.argv.index("--base") + 1] if "--base" in sys.argv else None
    if base in args:
        args.remove(base)
    if not args:
        sys.exit(__doc__)
    bot, others = args[0], args[1:]
    if not others:
        others = sorted(d for d in os.listdir(os.path.join(ROOT, "bots"))
                        if os.path.isdir(os.path.join(ROOT, "bots", d, "src")) and d != bot and not d.startswith(SKIP))
    if not build(bot):
        sys.exit(f"compare: {bot} does not build")
    out = tempfile.mkdtemp(prefix="compare-")
    rows = results(bot, others, out)
    was = results(base, others, out) if base and build(base) else {}
    shutil.rmtree(out, ignore_errors=True)

    head = f"{bot} against".ljust(18) + "".join(m.rjust(14) for m in MAPS) + "sum".rjust(14)
    print(head + (f"   change from {base}" if base else ""))
    total = [0, 0, 0]
    cuts = 0
    for o in others:
        r = rows[o]
        if r is None:
            print(f"  {o:<16}" + "  did not build or did not finish")
            continue
        d = [r[m][0] for m in MAPS]
        cuts += sum(r[m][1] for m in MAPS)
        total = [t + x for t, x in zip(total, d)]
        line = f"  {o:<16}" + "".join(f"{x:+14,d}" for x in d) + f"{sum(d):+14,d}"
        if base and was.get(o):
            line += f"   {sum(d) - sum(was[o][m][0] for m in MAPS):+,d}"
        lost = [m for m, x in zip(MAPS, d) if x < 0]
        print(line + (f"   loses {', '.join(lost)}" if lost else ""))
    print(f"  {'sum':<16}" + "".join(f"{x:+14,d}" for x in total) + f"{sum(total):+14,d}")
    print(f"  fuel cut-offs of {bot}: {cuts}" + ("   <-- a bug: find it before submitting" if cuts else ""))
    return 1 if cuts else 0


if __name__ == "__main__":
    sys.exit(main())
