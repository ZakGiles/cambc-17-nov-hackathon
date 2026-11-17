# Working on this repo

A three-hour, in-person, Battlecode-style competition in C++, in pairs, on
17 Nov 2026. The game is announced at the start. `README.md` has the layout
and the order of work; `RULES.md` (written on the day) has the game's facts.

## Working rules

- **Always keep the task list box up to date.**
- **Three hours.** Fast over tidy: no refactors, no abstractions for later,
  no tooling that takes longer to write than it saves today. Say how long a
  step will take before starting one over ten minutes.
- **A legal bot submitted comes first.** Before any strategy: read the
  rules, build the starter, submit it. From then on there is always a
  submitted bot that works.
- **Arena runs are quick: 5 to 10 games.** `tools/arena.py NEW OLD` plays 6.
  That size shows only a large difference (6-0 means something, 4-2 does
  not); never run hundreds. The replays are the point: every run ends with
  the command that opens each game, losses first. Give those to the users
  and say what to look for in each.
- **Watch before changing.** A change starts from something seen in a
  replay (a unit idle, a path failed, a fight lost), named with its game and
  round, not from a guess.
- **One change at a time.** Freeze the bot first (`cp -r bots/main
  bots/vN`), make the change, play new against frozen, keep it or delete it.
- **A crash or a timeout loses more than a heuristic wins.** Guard every
  index, every unit that may be dead, every empty list. Check the compute
  limit in the rules and stay far inside it.
- **Use `lib/kit` before writing an algorithm**; it is tested (`make test`).
  Copy it into the bot if the platform takes one directory.
- **Stop features 20 minutes before the end.** Submit the last version that
  beat its parent and check it runs on the platform.
- Commit at each version that works. Docs short and factual.
