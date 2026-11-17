# Working on this repo

Three-hour Battlecode-style competition in C++, in pairs, 17 Nov 2026. The game was announced at the start. The event is over: we came second. See `README.md`.

These are the rules the sessions worked by. The ones about submitting and the ladder applied during the event only.

## Rules

- **Write short, simple sentences.** In chat, docs and comments. Cut every word that is not needed.
- Keep the task list up to date.
- Fast over tidy. No refactors.
- There are only three maps. Every arena run plays all three. So check and iterate fast.
- Judge a version by score difference, not wins. Add up `ours - theirs` over the games.
- Compare before you push: `tools/compare.py BOT` plays it against every other bot in `bots/`. Read the rows, not only the sum. Mind the fuel cut-offs.
- `bots/main` has one owner at a time; work in your own `bots/NAME` and send the owner what won.
- Arena runs are 5 to 10 games. `tools/arena.py NEW OLD` plays 6.
- Show the replays, losses first. Say what to look for.
- Change something only after seeing it in a replay.
- One change at a time. Freeze first: `cp -r bots/main bots/vN`.
- A crash loses more than a heuristic wins. Guard indexes, dead units and empty lists.
- Hardcoding for maps is allowed. There are only three. Keep a plain fallback for a map we have not seen.
- Fuel is 20,000 a turn. A host call costs over 100. Count calls and loops before writing them.
- Any change to the fuel count shifts every later turn. Test it against the real opponent before it stays.
- Use `lib/kit` before writing an algorithm.
- Commit each version that works.

## During the event

- Submit a legal bot first. Always have a working bot submitted.
- Anyone on the site could read our live bot's name. So each submit took the next line of a prepared list as the name, never one that said what the bot does.
- One session owns the live bot. Tell the other sessions the result before you submit.
- Ladder tests: queue against every team. A match is only three maps.
- Stop features 20 minutes before the end.
