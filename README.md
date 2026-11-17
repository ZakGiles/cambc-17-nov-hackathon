# cambc-17-nov-hackathon

Three-hour competition in C++, in pairs, 17 Nov 2026. The game is PHAIS (Milkwars): botlets drag cows onto scoring cells. Rules: `RULES.md`.

## Install

Event site: https://dock-welfare-against-gpl.trycloudflare.com (the link may change; ask the organiser).

1. Clone this repo.
2. Open the site's CLI page. Download `phais` for your system.
3. Put it on your PATH.
   - macOS, Linux: `chmod +x phais`, then `mv phais ~/.local/bin/`.
   - macOS only: `xattr -d com.apple.quarantine ~/.local/bin/phais`.
   - Windows: put `phais.exe` in a folder on your PATH.
4. Check: `phais --version` prints `phais 0.1.0`.
5. In the repo: `phais build starter`. The first build fetches the compiler.
6. Play: `phais run starter starter --maps maps/ --view`.
7. To submit: make or join the team on the site, then `phais login`. Then see Submit.

The arena needs Python 3: `python3 tools/arena.py main starter`.

## Layout

- `bots/main`: our bot. `bots/vN`: frozen copies. `bots/starter`: the site's example.
- `maps/`: the server's three maps, for offline games.
- `RULES.md`: the game's facts.
- `tools/arena.py`, `arena.json`: quick games with replays.
- `tools/gen_maps.py`: writes `bots/main/src/maps.h` from `maps/`. Run it after a map changes.
- `lib/kit/`: tested C++20 headers. Run `make test`.
- `lib/kitcambc/`: algorithms from the cambc bot. Not tested.
- `sdk/`, `cmake/`, `phais.toml`: made by `phais init`.
- `site/`: files saved from the event site.

## Commands

```bash
phais build main
phais run main starter --maps maps/ --view   # all three maps
phais view                                   # the newest replay
```

## Ladder with Battlecode ranks

`tools/ladder_proxy.py` serves the event site on localhost. Each team on the ladder gets a tier (Bronze to Challenger) from its Elo here. Teams on the [Battlecode ladder](https://2026a.battlecode.co.uk/game/ladder) also get its badges: rank, finalist, climbing streak, category and flags. Icons, logo and flags are Battlecode's own. The data is in `site/bc_ladder.json` and `site/bc_assets/`.

```bash
tools/ladder_proxy.py                 # then open http://localhost:8017/#/ladder
tools/ladder_proxy.py --fetch         # copy the Battlecode ladder again
tools/ladder_proxy.py --upstream URL  # the event site link changed
```

Names match without case, spaces or symbols. "A & B" gets both ranks. A unique prefix also matches. No badge when no team matches or two share the name.

## Submit

Anyone on the site can read our live bot's name. So the name must not say what the bot does.

```bash
tools/submit.py main --dry   # show the next name
tools/submit.py main         # phais submit main --name "<next name>"
```

Names come from `names.txt`, one per line, top first. Each line is used once. The server cuts a name to 48 characters. `names.used` holds the used line numbers. Before it picks a name, the script reads `names.used` on origin/main too, since the other clone may have submitted. After each submit, it commits and pushes `names.used` to main.

Submitting makes the new version live. `phais submissions live N` puts an old one back.

## Loop

1. Watch a replay. Pick the biggest problem.
2. `cp -r bots/main bots/vN`. Change `bots/main`.
3. `tools/arena.py main vN`. Watch the losses.
4. Keep and submit, or go back to `vN`.

Last 20 minutes: no new features. Submit the best version.

## Arena

```bash
tools/arena.py main v1              # 6 games: 3 maps, both seats
tools/arena.py main v1 --games 10
tools/arena.py main v1 --open       # open the first replay
```

It prints each score, the score difference (ours - theirs) per game, map, seat and in total, and a command to open each replay, worst first. Replays go to `replays/arena/`.

Six games only show a big difference.

There are only three maps, so every run covers all of them. Check and iterate fast.

Judge by score difference, not wins. Add up `ours - theirs` over the games. A 3-3 split can hide a big gain or loss.

## Kit

A bot is one folder, so copy what you use: `cp -r lib/kit bots/main/src/kit`.

| header | what |
| --- | --- |
| `grid.h` | points, directions, distances, `Grid<T>` |
| `field.h` | BFS and Dijkstra fields, `step`, `path`, `regions` |
| `assign.h` | units to targets: `assign_min`, `assign_greedy` |
| `gamesearch.h` | `Mcts`, `AlphaBeta` |
| `rng.h` | seeded random numbers |
| `deadline.h` | time left in a turn |
| `svec.h` | fixed-size vector |

`lib/kitcambc/` has `walk.h`, `fight.h`, `plan.h` and `atlas.h` from the 2026a bot.

The fuel limit is 20,000 a turn. Most of the kit costs too much to run every turn.
