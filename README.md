# cambc-17-nov-hackathon

Three-hour Battlecode-style competition in C++, in pairs, 17 Nov 2026. The game is announced at the start.

## Layout

- `lib/kit/`: tested C++20 headers. Run `make test`.
- `tools/arena.py`, `arena.json`: quick games with replays.
- `bots/main`: our bot. `bots/vN`: frozen copies.
- `RULES.md`: the game's facts. Written on the day.

## On the day

Start:

1. Read the rules. Write `RULES.md`.
2. Build the starter. Watch one replay.
3. Submit the starter.
4. Fill in `arena.json`.

Loop:

1. Watch a replay. Pick the biggest problem.
2. `cp -r bots/main bots/vN`. Change `bots/main`.
3. `tools/arena.py main vN`. Watch the losses.
4. Keep and submit, or go back to `vN`.

Last 20 minutes: no new features. Submit the best version.

## Arena

```bash
tools/arena.py main v1              # 6 games, both seats
tools/arena.py main v1 --games 10
tools/arena.py main v1 --open       # open the first replay
```

It prints the score and a command to open each replay, losses first. Replays go to `replays/arena/`.

Six games only show a big difference.

`arena.json` is set for the `cambc` CLI. For another engine, change `cmd`, `first`, `second` and `view`. See `tools/arena.py --help`.

## Kit

| header | what |
| --- | --- |
| `grid.h` | points, directions, distances, `Grid<T>` |
| `field.h` | BFS and Dijkstra fields, `step`, `path`, `regions` |
| `assign.h` | units to targets: `assign_min`, `assign_greedy` |
| `gamesearch.h` | `Mcts`, `AlphaBeta` |
| `rng.h` | seeded random numbers |
| `deadline.h` | time left in a turn |
| `svec.h` | fixed-size vector |

```cpp
#include "kit/assign.h"
#include "kit/field.h"

kit::Field to_goal;  // one search serves every unit
to_goal.bfs(w, h, goals, [&](kit::P p) { return !wall[p]; });
kit::P next = to_goal.step(unit, [&](kit::P p) { return !taken[p]; });

auto job = kit::assign_min(units.size(), targets.size(),
                           [&](int u, int t) { return field[t][units[u]]; });
```

Build with `-std=c++20 -Ilib`. To submit one directory: `cp -r lib/kit bots/main/src/kit`.

## If it uses the `cambc` CLI

```bash
cambc init --yes
cambc new main --lang cpp
cambc run main starter-cpp --view
cambc login && cambc submit main
```
