#!/usr/bin/env python3
"""A few quick games of bot A against bot B, with every replay kept for watching.

  tools/arena.py main v1                 # 6 games: 3 maps, each from both seats
  tools/arena.py main v1 --games 10 --maps 'maps/big*'
  tools/arena.py main v1 --open          # then open the first replay on the watch list

How a game is played, who won it and how a replay is opened come from
arena.json (any key can be overridden with a flag of the same name):

  prepare shell command run once before the games, such as a build, so that
          games at once do not build the same bot together. Fields: {a} {b}
  cmd     shell command for one game. Fields: {first} {second} the bots'
          directories by seat, {map}, {seed}, {replay} where the replay must
          land, {dir} its directory, {stem} its name without the extension.
  first   regex found in the command's output when the first seat won
  second  the same for the second seat
  score   instead of first and second: a regex with two groups, the first
          seat's score and the second's. The higher score wins.
  note    a regex; what it matches in the output is printed beside the game
  maps    glob of map files ("" for a game with no maps)
  ext     replay file extension
  view    shell command that opens one replay. Field: {replay}
  cwd     directory to run in, relative to the repository (default ".")
  games, jobs, timeout   as the flags
  stagger seconds between one game's start and the next, for an engine whose
          start-up must not overlap (a build check in the bot's directory)

Six to ten games only show a large difference: a 4-2 means nothing, a 6-0
means something. The point of a run this size is the replays.
"""
import argparse
import glob
import json
import os
import re
import shlex
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEYS = ("prepare", "cmd", "first", "second", "score", "note", "maps", "ext", "view", "cwd")


def settings():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a", help="our bot: a name under bots/ or a path")
    ap.add_argument("b", help="the opponent, likewise")
    ap.add_argument("--games", type=int, help="default 6")
    ap.add_argument("--jobs", type=int, help="games at once (default 3)")
    ap.add_argument("--timeout", type=float, help="seconds a game may take (default 600)")
    ap.add_argument("--stagger", type=float, help="seconds between game starts (default 0)")
    ap.add_argument("--out", default="replays/arena", help="where replays and logs go")
    ap.add_argument("--config", default=os.path.join(ROOT, "arena.json"))
    ap.add_argument("--open", action="store_true", help="open the first replay on the watch list")
    for key in KEYS:
        ap.add_argument("--" + key)
    args = ap.parse_args()
    conf = {"prepare": "", "first": "", "second": "", "score": "", "note": "", "maps": "", "ext": ".replay", "view": "", "cwd": ".", "games": 6, "jobs": 3, "timeout": 600, "stagger": 0}
    if os.path.exists(args.config):
        with open(args.config) as f:
            conf.update({k: v for k, v in json.load(f).items() if not k.startswith("_")})
    for key in KEYS + ("games", "jobs", "timeout", "stagger"):
        if getattr(args, key) is not None:
            conf[key] = getattr(args, key)
    missing = [k for k in (("cmd",) if conf.get("score") else ("cmd", "first", "second")) if not conf.get(k)]
    if missing:
        sys.exit(f"arena: set {', '.join(missing)} in {args.config} or as flags (see --help)")
    return args, conf


def bot_dir(name, cwd):
    for base in (os.path.join(cwd, "bots", name), os.path.join(cwd, name), name):
        if os.path.exists(base):
            return os.path.abspath(base)
    sys.exit(f"arena: no bot {name!r} (looked in {os.path.join(cwd, 'bots')})")


start_lock, next_start = threading.Lock(), [0.0]


def play(game, conf, cwd):
    fields = {k: shlex.quote(str(v)) for k, v in game["fields"].items()}
    cmd = conf["cmd"].format(**fields)
    timeout = conf["timeout"]
    with start_lock:
        time.sleep(max(0.0, next_start[0] - time.time()))
        next_start[0] = time.time() + conf["stagger"]
    start = time.time()
    try:
        done = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True, errors="replace", timeout=timeout)
        text, note = done.stdout + done.stderr, "" if done.returncode == 0 else f"exit {done.returncode}"
    except subprocess.TimeoutExpired as e:
        text = "".join(x.decode(errors="replace") if isinstance(x, bytes) else (x or "") for x in (e.stdout, e.stderr))
        note = f"timeout {timeout:.0f}s"
    game["seconds"] = time.time() - start
    with open(game["log"], "w") as f:
        f.write(f"$ {cmd}\n{text}")
    scored = re.search(conf["score"], text) if conf["score"] else None
    if conf["score"]:
        if not scored:
            game["result"], game["note"] = "ERROR", note or "no score in the output"
            return game
        one, two = int(scored.group(1)), int(scored.group(2))
        mine, theirs = (one, two) if game["a_first"] else (two, one)
        game["result"] = "WIN" if mine > theirs else "LOSS" if mine < theirs else "draw"
        extra = re.search(conf["note"], text) if conf["note"] else None
        game["note"] = f"{mine}-{theirs}" + (f"  {extra.group(0)}" if extra else "") + (f"  {note}" if note else "")
        return game
    first, second = re.search(conf["first"], text), re.search(conf["second"], text)
    if bool(first) == bool(second):
        game["result"] = "ERROR" if note or first else "draw"
        game["note"] = note or ("both seats matched" if first else "no winner in the output")
    else:
        game["result"] = "WIN" if bool(first) == game["a_first"] else "LOSS"
        game["note"] = note
    return game


def main():
    args, conf = settings()
    cwd = os.path.normpath(os.path.join(ROOT, conf["cwd"]))
    a, b = bot_dir(args.a, cwd), bot_dir(args.b, cwd)
    maps = sorted(glob.glob(os.path.join(cwd, conf["maps"]))) if conf["maps"] else [""]
    if not maps:
        sys.exit(f"arena: no maps match {conf['maps']!r} in {cwd}")
    out = os.path.join(ROOT, args.out, time.strftime("%H%M%S") + f"-{os.path.basename(a)}-v-{os.path.basename(b)}")
    os.makedirs(out, exist_ok=True)

    # Games come in pairs: the same map and seed from each seat.
    games = []
    for i in range(conf["games"]):
        pair, a_first = i // 2, i % 2 == 0
        map_path = maps[pair % len(maps)]
        map_name = os.path.splitext(os.path.basename(map_path))[0] or "game"
        stem = f"{i + 1:02d}-{map_name}-{'first' if a_first else 'second'}"
        replay = os.path.join(out, stem + conf["ext"])
        games.append({
            "n": i + 1, "map": map_name, "a_first": a_first, "replay": replay, "log": os.path.join(out, stem + ".log"),
            "fields": {"first": a if a_first else b, "second": b if a_first else a, "map": map_path,
                       "seed": pair // len(maps) + 1, "replay": replay, "dir": out, "stem": stem},
        })

    if conf["prepare"]:
        built = subprocess.run(conf["prepare"].format(a=shlex.quote(a), b=shlex.quote(b)), shell=True, cwd=cwd,
                               capture_output=True, text=True, errors="replace")
        if built.returncode != 0:
            sys.exit(f"arena: prepare failed\n{(built.stdout + built.stderr)[-3000:]}")
    print(f"{os.path.basename(a)} against {os.path.basename(b)}: {len(games)} games, {conf['jobs']} at once")
    start = time.time()
    with ThreadPoolExecutor(max_workers=max(1, conf["jobs"])) as pool:
        for g in as_completed([pool.submit(play, g, conf, cwd) for g in games]):
            g = g.result()
            seat = "first" if g["a_first"] else "second"
            print(f"  {g['n']:2d}  {g['map']:<16} {seat:<6}  {g['result']:<5}  {g['seconds']:5.1f}s  {g['note']}", flush=True)

    count = {r: sum(g["result"] == r for g in games) for r in ("WIN", "LOSS", "draw", "ERROR")}
    print(f"\n{os.path.basename(a)} won {count['WIN']}, lost {count['LOSS']}, drew {count['draw']} of {len(games)}"
          + (f", {count['ERROR']} failed" if count["ERROR"] else "") + f"  ({time.time() - start:.0f}s)")
    for seat, flag in (("first", True), ("second", False)):
        mine = [g for g in games if g["a_first"] == flag]
        print(f"  moving {seat}: {sum(g['result'] == 'WIN' for g in mine)} of {len(mine)}")

    # Losses teach the most, so they head the list.
    rank = {"ERROR": 0, "LOSS": 1, "draw": 2, "WIN": 3}
    watch = sorted(games, key=lambda g: (rank[g["result"]], g["n"]))
    rel = lambda p: os.path.relpath(p, os.getcwd())
    print("\nWatch (losses first):")
    for g in watch:
        if g["result"] == "ERROR":
            print(f"  {g['n']:2d} ERROR  {rel(g['log'])}")
            continue
        seen = os.path.exists(g["replay"])
        show = conf["view"].format(replay=shlex.quote(rel(g["replay"]))) if conf["view"] else rel(g["replay"])
        print(f"  {g['n']:2d} {g['result']:<5}  {show}" + ("" if seen else "   (no replay file: see " + rel(g["log"]) + ")"))
    if args.open and conf["view"]:
        first = next((g for g in watch if os.path.exists(g["replay"])), None)
        if first:
            subprocess.Popen(conf["view"].format(replay=shlex.quote(first["replay"])), shell=True, cwd=cwd)
    return 1 if count["ERROR"] else 0


if __name__ == "__main__":
    sys.exit(main())
