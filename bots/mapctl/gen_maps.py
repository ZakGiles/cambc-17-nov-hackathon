#!/usr/bin/env python3
"""Writes bots/main/src/maps.h: per-map tables, so the bot spends no fuel on them.

  python3 tools/gen_maps.py

For each cell: productivity, starting cows, the way to drag a cow from it, and
how much that step is worth. The worth comes from a potential: a cell's own
value, or a better cell's value less 2.5 a step. So cows deep in the enemy's
area get dragged out, and cows near ours get dragged in.
"""
import json, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # mapctl copy: the repo
MAPS = ["checkerboard", "frontier", "lair"]  # the order is the bot's map number
N = 14
DIRS = [(0, 1), (1, 0), (0, -1), (-1, 0)]  # N E S W, as the SDK numbers them
NEAR = 8
# Potential lost a step, in half points. High: a cow only goes to a better cell
# close by. Low: flat spots do not stop a cow on its way up a slope.
STEP = {"checkerboard": 5, "frontier": 1, "lair": 5}


def lair_phi(p):
    """Lair: push enemy cows out of their corner and on, until four steps out.
    A bot like ours takes back only cows within three steps of its corner."""
    def dist_to(i, keep):
        x, y = i % N, i // N
        return min(abs(j % N - x) + abs(j // N - y) for j in range(N * N) if keep(p[j]))
    phi = []
    for i in range(N * N):
        if p[i] > 0:
            phi.append(2 * p[i])
        elif p[i] < 0:
            phi.append(-30 - 5 * (dist_to(i, lambda v: v == 0) - 1))  # edge -30, deeper lower
        else:
            de = dist_to(i, lambda v: v < 0)
            phi.append(-5 * (4 - de) if de <= 3 else max(0, 20 - 5 * dist_to(i, lambda v: v > 0)))
    return phi


def ways(phi):
    way, gain = [], []
    for i in range(N * N):
        x, y = i % N, i // N
        bd, bg = 4, 0
        for d in (2, 3, 0, 1):  # ties go home: S, W first
            nx, ny = x + DIRS[d][0], y + DIRS[d][1]
            if 0 <= nx < N and 0 <= ny < N and phi[ny * N + nx] - phi[i] > bg:
                bd, bg = d, phi[ny * N + nx] - phi[i]
        way.append(bd)
        gain.append(min(bg, 120))
    return way, gain


def tables(m):
    step = STEP[m["id"]]
    p = m["productivity"]
    cows = [0] * (N * N)
    for c in m["cows"]:
        cows[c["y"] * N + c["x"]] += 1
    phi = []
    for i in range(N * N):
        x, y = i % N, i // N
        best = 2 * p[i]
        for j in range(N * N):
            d = abs(j % N - x) + abs(j // N - y)
            best = max(best, 2 * p[j] - step * d)
        phi.append(best)
    base_way = ways(phi)[0]  # main's table: the lock guesses enemy drags from it
    if m["id"] == "lair":
        phi = lair_phi(p)
    way, gain = ways(phi)
    order = [i for i in range(N * N) if cows[i]]
    where = [255] * (N * N)
    for k, i in enumerate(order):
        where[i] = k
    hot = [i for i in range(N * N) if gain[i] > 0]  # cells worth a look: a cow there can be dragged
    # For each cell, the eight nearest cells worth a look, nearest first: a target search is eight checks.
    near = []
    for i in range(N * N):
        x, y = i % N, i // N
        cand = sorted((abs(j % N - x) + abs(j // N - y), -gain[j], j) for j in hot if j != i)
        near.append([j for d, g, j in cand if d <= 4][:NEAR] + [255] * NEAR)
        near[-1] = near[-1][:NEAR]
    return p, cows, way, gain, order + [0] * (N * N - len(order)), where, hot + [0] * (N * N - len(hot)), len(order), len(hot), near, base_way


def row(xs):
    return "{" + ",".join(str(x) for x in xs) + "}"


def main():
    t = [tables(json.load(open(os.path.join(ROOT, "maps", name + ".json")))) for name in MAPS]
    names = [("signed char", "kP"), ("unsigned char", "kC"), ("unsigned char", "kWay"), ("signed char", "kGain"),
             ("unsigned char", "kList"), ("unsigned char", "kWhere"), ("unsigned char", "kHot")]
    out = ["// Made by tools/gen_maps.py from maps/. Do not edit.", "#pragma once", "",
           f"constexpr int kMaps = {len(MAPS)};  // " + ", ".join(MAPS)]
    for k, (ty, name) in enumerate(names):
        out.append(f"constexpr {ty} {name}[kMaps][196] = {{")
        out += ["    " + row(m[k]) + "," for m in t]
        out.append("};")
    out.append("constexpr int kListN[kMaps] = " + row(m[7] for m in t) + ";")
    out.append("constexpr int kHotN[kMaps] = " + row(m[8] for m in t) + ";")
    out.append("constexpr unsigned char kWayBase[kMaps][196] = {")
    out += ["    " + row(m[10]) + "," for m in t]
    out.append("};")
    out.append(f"constexpr int kNearN = {NEAR};")
    out.append("constexpr unsigned char kNearCells[kMaps][196][kNearN] = {")
    out += ["    {" + ",".join(row(c) for c in m[9]) + "}," for m in t]
    out.append("};")
    open(os.path.join(ROOT, "bots", "mapctl", "src", "maps.h"), "w").write("\n".join(out) + "\n")
    for name, m in zip(MAPS, t):
        print(name, "cells with cows", m[7], "cells worth a drag", m[8])


if __name__ == "__main__":
    main()
