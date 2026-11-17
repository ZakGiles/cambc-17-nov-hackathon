# cambc-17-nov-hackathon

A three-hour, in-person, Battlecode-style competition in C++, in pairs, on 17 Nov 2026. The game is announced at the start, so everything here works for any game: a tested C++ kit, an arena for quick games with replays, and the order of work.

## Layout

- `lib/kit/`: header-only C++20, no dependencies. `make test` checks it against slow reference answers; `make wasm-check` compiles it with the toolchain `cambc build` uses.
- `tools/arena.py`, `arena.json`: a few games of one bot against another, every replay kept.
- `tests/test_kit.cpp`: the kit's tests.
- `bots/<name>/`: one directory a bot (made on the day). `bots/main` is the one we work on; `bots/vN` are frozen copies to play against.
- `RULES.md` (made on the day): the game's facts, one line each.

## On the day

First 20 minutes:

1. Read the rules to the end. Write `RULES.md`: how a game is won, the tiebreak, what a turn may do, the compute limit, what a unit can see, map sizes.
2. Build the starter bot, play it against itself, open the replay.
3. Submit the starter. Now a legal bot is in.
4. Fill in `arena.json` (below) and play `tools/arena.py main main`.

Then, until 20 minutes before the end:

1. Watch a replay. Pick the largest thing going wrong.
2. `cp -r bots/main bots/vN`, change `bots/main`.
3. `tools/arena.py main vN`: 6 games. Watch the losses.
4. Keep and submit, or `rm -rf bots/main && cp -r bots/vN bots/main`.

Last 20 minutes: no new features. Submit the last version that beat its parent and check it runs on the platform.

## Arena

```bash
tools/arena.py main v1                # 6 games: 3 maps, each from both seats
tools/arena.py main v1 --games 10
tools/arena.py main v1 --open         # then open the first replay on the watch list
```

A run prints each game as it ends, the tally, and a watch list: the command that opens each replay, losses first. Replays and each game's output are under `replays/arena/<time>-<a>-v-<b>/`.

Six to ten games show only a large difference. Use the run to find games worth watching, not to measure a few percent.

`arena.json` says how a game is played, who won and how a replay is opened (`tools/arena.py --help` lists the keys). It is set for the `cambc` CLI 5.0.0 (checked with 6 real games), which is a guess at what the day uses. For another engine, change `cmd` to its command with `{first} {second} {map} {seed} {replay}`, `first` and `second` to the text it prints when each seat wins, and `view` to its viewer.

## The kit

| header | what |
| --- | --- |
| `grid.h` | `P` (x, y), `kDirs8` (the first four are orthogonal), `manhattan`, `cheb`, `dist2`, `Grid<T>` |
| `field.h` | `Field`: `bfs` and `dijkstra` distance fields from one or many sources, `step` to the lowest neighbour, `path`; `regions` |
| `assign.h` | `assign_min` (the best matching of units to targets), `assign_greedy` |
| `gamesearch.h` | `Mcts` and `AlphaBeta` for two-player games, bounded by time |
| `rng.h` | `Rng`: seeded, `below`, `range`, `unit`, `chance`, `shuffle`, `pick` |
| `deadline.h` | `Deadline(ms)`: `passed()`, `left_ms()` |
| `svec.h` | `SVec<T, N>`: a fixed-size vector with no heap |

```cpp
#include "kit/assign.h"
#include "kit/field.h"

kit::Field to_goal;                    // one search from the goals serves every unit
to_goal.bfs(w, h, goals, [&](kit::P p) { return !wall[p]; });
kit::P next = to_goal.step(unit, [&](kit::P p) { return !taken[p]; });

std::vector<int> job = kit::assign_min(units.size(), targets.size(),
                                       [&](int u, int t) { return field[t][units[u]]; });
```

Native builds: `-std=c++20 -Ilib`. If the platform takes one directory per bot (as `cambc submit` does), copy the kit in: `cp -r lib/kit bots/main/src/kit`; the includes stay `"kit/field.h"`.

## If it runs on the `cambc` CLI

```bash
cambc init --yes                        # sdk/, cmake/, maps/, bots/starter-cpp
cambc new main --lang cpp
cambc run main starter-cpp --view
cambc login && cambc submit main
```

`../cambc` is the 2026a repo: `handbook/` there has that game's rules and API, `README.md` its fuel costs.
