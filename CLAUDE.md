# Working on this repo

Three-hour Battlecode-style competition in C++, in pairs, 17 Nov 2026. The game is announced at the start. See `README.md`.

## Rules

- **Write short, simple sentences.** In chat, docs and comments. Cut every word that is not needed.
- Keep the task list up to date.
- Fast over tidy. No refactors.
- Submit a legal bot first. Always have a working bot submitted.
- Arena runs are 5 to 10 games. `tools/arena.py NEW OLD` plays 6.
- Show the replays, losses first. Say what to look for.
- Change something only after seeing it in a replay.
- One change at a time. Freeze first: `cp -r bots/main bots/vN`.
- A crash loses more than a heuristic wins. Guard indexes, dead units and empty lists.
- Hardcoding for maps is allowed. There are only three. Keep a plain fallback for a map we have not seen.
- Fuel is 20,000 a turn. A host call costs over 100. Count calls and loops before writing them.
- Use `lib/kit` before writing an algorithm.
- Stop features 20 minutes before the end.
- Commit each version that works.
