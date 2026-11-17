# PHAIS rules

From the site's CLI page, 17 Nov. Marked "provisional" there. `?` = not checked in a game yet.

## Game

- Board 14×14. Coordinates 0 to 13.
- 8 botlets a team. One program moves all 8.
- We are bottom-left. The enemy is top-right. The API turns the board for us.
- Default spawns: all 8 on (0,0), theirs on (13,13).
- Maps turn 180° onto themselves. Pool so far: checkerboard, frontier, lair.
- All moves happen at once.

## Score

- Each cell has a productivity from -10 to 10.
- Positive cells are ours, near our corner. Negative cells are theirs. `?` sign from team B's view.
- Each turn a cow adds its cell's value to the side that owns the cell.
- Scores add up every turn. `?` most points wins.

## Moves

- A botlet moves one cell: N (+y), E (+x), S (-y), W (-x). No command means stay.
- A move off the board stays put.
- A botlet can drag one cow from its own cell along its move.
- `drag_index` picks the cow by ID order on that cell. -1 means no drag.
- Pulls on one cow add up, from both teams. The sum is cut at the edge.
- Two botlets pulling the same way move the cow two cells.
- Opposite pulls cancel.
- Every botlet that dragged a cow ends on the cow's final cell.

## Limits

- 20,000 fuel a turn for the whole team. The first turn is the same.
- Out of fuel ends the turn early. It is not a forfeit.
- A trap or an exit forfeits. `main` must never return.
- State stays in memory between turns.

## API (`phais.hpp`)

- `get_width()`, `get_height()`, `get_turn()`, `get_max_turns()`, `get_seed()`
- `get_score(team)`: 0 us, 1 enemy
- `get_productivity(x, y)`: -1 out of bounds
- `get_bot_x(team, i)`, `get_bot_y(team, i)`: i from 0 to 7
- `get_cows(x, y)`: cows on a cell
- `submit_move(botlet, Direction, drag_index = -1)`: 0 ok, -1 invalid
- `end_turn()`

## CLI

- `phais init`, `phais new NAME`, `phais build NAME`
- `phais run A B` plays a series on the server's maps. `--maps DIR` for local maps.
- `phais view` opens the latest replay.
- `phais login`, then `phais submit NAME`. Make the team on the website first.

## Maps (`maps/`, from the server on 17 Nov)

- checkerboard: a cow on every cell. Cells are +10 or -10 in 2×2 blocks. A cow is always next to a cell of the other sign.
- frontier: 28 cows, two on each cell of the diagonal from top-left to bottom-right. Values fall evenly from +10 at our corner to -10 at theirs.
- lair: each team has a 4×4 corner of ±10 with three cows a cell (48 cows). Everything else is 0.
- All three: spawns on (0,0) and (13,13).

## Server

- Compiles on the server: Linux x64. Source up to 2 MB.
- `site/` has the SDK header, the example bot, the map pool and the server info as saved.
