#!/usr/bin/env python3
"""What happened in a replay, in numbers.

  tools/stats.py replays/arena/<run>/01-lair-first.json [more.json ...]

Each row is a stretch of turns. For side A then B: points a turn, botlets
standing still a turn, and cow steps a turn that gained that side points.
"""
import json, sys

N = 14


def main():
    for path in sys.argv[1:]:
        d = json.load(open(path))
        fr = d["frames"]
        p = fr[0]["productivity"]
        names = [b["name"] for b in d["bots"]]
        print(f"{path}\n  {d['map']['id']}: {names[0]} (A) {d['result']['scores'][0]} - {d['result']['scores'][1]} {names[1]} (B)"
              f"   fuel cut-offs {d['fuel_exhausted']}")
        print("  turns      A pts/t  B pts/t   A still  B still   A gain/t  B gain/t   cows + / 0 / -")
        span = max(1, (len(fr) - 1) // 10)
        for t0 in range(0, len(fr) - 1, span):
            t1 = min(t0 + span, len(fr) - 1)
            n = t1 - t0
            pts = [(fr[t1]["scores"][s] - fr[t0]["scores"][s]) / n for s in (0, 1)]
            still = [0, 0]
            gain = [0, 0]
            for t in range(t0, t1):
                for s in (0, 1):
                    still[s] += sum(1 for a, b in zip(fr[t]["botlets"][s], fr[t + 1]["botlets"][s]) if a == b)
                was = {c["id"]: p[c["y"] * N + c["x"]] for c in fr[t]["cows"]}
                for c in fr[t + 1]["cows"]:
                    dv = p[c["y"] * N + c["x"]] - was[c["id"]]
                    if dv > 0: gain[0] += dv
                    if dv < 0: gain[1] -= dv
            end = [p[c["y"] * N + c["x"]] for c in fr[t1]["cows"]]
            print(f"  {t0:3d}-{t1:3d}  {pts[0]:8.1f} {pts[1]:8.1f}  {still[0] / n:7.2f} {still[1] / n:8.2f}  {gain[0] / n:9.1f} {gain[1] / n:9.1f}"
                  f"   {sum(v > 0 for v in end)} / {sum(v == 0 for v in end)} / {sum(v < 0 for v in end)}")


if __name__ == "__main__":
    main()
