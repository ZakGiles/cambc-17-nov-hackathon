# cambc-17-nov-hackathon

Our entry to a three-hour bot competition, in pairs, in C++, on 17 Nov 2026. The game was announced at the start. We came **second of 11** in the final tournament.

Team: Sponge(AlbertAndBob). Two people, working with Claude Code. Several sessions ran in parallel on two machines.

## The game

PHAIS (Milkwars). Two teams of 8 botlets drag cows across a 14×14 board. Each turn a cow scores the value of the cell it stands on, for the side that owns the cell. A game is 500 turns. Three maps: checkerboard, frontier, lair. A match is all three, and the side with more map wins takes it.

One program moves all 8 botlets. It gets 20,000 fuel a turn. A read costs about 133, a move about 410. So a turn affords 8 moves, about 50 reads and a few short loops.

The facts we checked are in [RULES.md](RULES.md).

## Result

Single elimination, 11 teams. We were seeded third.

| round | opponent | maps | checkerboard | frontier | lair |
| --- | --- | --- | --- | --- | --- |
| Quarter-final | bb2 | won 2-1 | 482,100 - 497,900 | 89,316 - 20,462 | 200,080 - 59,710 |
| Semi-final | Swedish fish | won 2-1 | 338,280 - 641,720 | 56,949 - 49,722 | 79,430 - 48,800 |
| Final | Mite & Claudus Maximus | lost 1-2 | 528,940 - 451,060 | 22,181 - 80,641 | 129,070 - 174,260 |

## Layout

- `bots/main`: the final bot.
- `bots/starter`: the site's example bot.
- `bots/v1`, `v10`, `v16`: earlier versions of main.
- `bots/s2`, `s12`, `s19`, `s26`, `s28`: steps on the second line of work, where the lock and the opening tables came from.
- `bots/gen`, `mapctl`, `claude`: side experiments, kept as opponents for local tests.
- `maps/`: the three maps.
- `tools/`: local games and statistics.
- `lib/kit/`: small tested C++20 headers we prepared before the event. `lib/kitcambc/`: algorithms from an earlier bot, not tested.
- `sdk/`, `cmake/`, `phais.toml`: made by `phais init`.
- [CLAUDE.md](CLAUDE.md): the working rules we gave the Claude Code sessions.

## Run it

The bots build with the event's `phais` CLI, version 0.1.0. It came from the event site and is not in this repo.

```bash
phais build main
phais run main starter --maps maps/ --view   # all three maps
phais view                                   # the newest replay
```

The tools need Python 3.

```bash
tools/compare.py main                 # main against every other bot in bots/
tools/compare.py main s26 --noise 10  # ten nudged games a map
tools/arena.py main v10               # 6 games: 3 maps, both seats
tools/stats.py REPLAY.json            # points, idle botlets and gains per stretch of turns
tools/gen_maps.py main                # rebuild maps.h from maps/
make test                             # the lib/kit tests
```

`compare.py` prints one row per opponent: the score difference (ours - theirs) on each map. Judge by that difference, not by wins. It also flags fuel cut-offs.
