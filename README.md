# cambc-17-nov-hackathon

Our entry to a three-hour bot competition, in pairs, in C++, on 17 Nov 2026. The game was announced at the start. We came **second of 11** in the final tournament.

Team: Sponge(AlbertAndBob). Two people, working with Claude Code. On one machine several sessions ran in parallel.

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

Our score is first. On the ladder we led for a stretch and were second at the close.

## How the bot works

The final bot is [bots/main](bots/main/src/main.cpp).

- **Tables, not search.** The three maps were known, so `tools/gen_maps.py` works out, for every cell, which way to drag a cow and where the nearest work is. The result is [maps.h](bots/main/src/maps.h). At run time a botlet drags the cow on its cell one step up the slope, or walks to the nearest cow worth dragging.
- **Fuel count.** A turn that runs out of fuel resumes next turn with stale positions. The bot counts what it spends and decides before it moves.
- **Rides.** Pulls on one cow add up, so several botlets pulling one cow travel several cells a turn. Used to reach far work.
- **Lock and dodge.** Opposite pulls cancel. A botlet can pull against the drag an enemy is about to make on our cows. When our own drag gets cancelled, the botlet takes another cow or another way.
- **Opening tables.** At turn 4 or 6 the bot reads where the eight enemy botlets stand. If the shape matches a known opponent, it switches to settings tuned against that opponent on the server.
- **Lair raid.** Some teams rushed all eight botlets into our corner and left their own empty. Against that opening, two of ours walk to their corner, drag their cows out and stay there. It beat one opponent's rush 2-1 on the ladder and made lair worse against that team's next bot, so it fires on exact openings only. The Swedish fish opening went in one minute before the close, with no ladder test. Earlier that hour they had taken lair 235,010 to 114,610. In the semi-final we took it 79,430 to 48,800.
- **Unseen map.** A plain fallback reads the map in counted steps, in case the pool changed.

## What we learned

- **Fuel was the real limit.** Every idea had to fit in a few hundred fuel.
- **Games are deterministic and chaotic.** The same two bots give the same score every time, yet a change that should not matter can move a map by 80,000. One game is one sample. `tools/compare.py --noise K` plays each map K times with one cow pair nudged.
- **Our own old bots were a poor test.** A change that beat every local bot could still lose on the ladder. Only games against the real opponents settled it.
- **A correct fix can undo tuning.** Counting the opening reads as fuel removed a cut-off on lair turn 4. The settings had been tuned with that cut-off in play, and the fixed bot lost 0-3 where the old one won 2-1.
- **Counters age fast.** A counter keyed to one opening stops working when the opponent changes it. The top teams changed bots every few minutes near the end.
- **One owner for the live bot.** With two people and many sessions submitting, the live bot changed hands about ten times in an hour.

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
