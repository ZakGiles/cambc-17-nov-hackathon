#!/usr/bin/env python3
"""Submit a bot under the next unused name in names.txt.

  tools/submit.py main          # phais submit main --name "<next name>"
  tools/submit.py main --dry    # print the next name only

names.txt holds one name per line, top first. Blank lines are skipped.
Each name is used once. Used names go to names.used.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = os.path.join(ROOT, "names.txt")
USED = os.path.join(ROOT, "names.used")


def lines(path):
    if not os.path.exists(path):
        return []
    with open(path) as f:
        return [s.strip() for s in f if s.strip()]


def main():
    dry = "--dry" in sys.argv[1:]
    bots = [a for a in sys.argv[1:] if a != "--dry"]
    if len(bots) != 1:
        sys.exit(__doc__)
    used = set(lines(USED))
    name = next((n for n in lines(NAMES) if n not in used), None)
    if name is None:
        sys.exit(f"submit: no unused names left in {NAMES}")
    print(f"name: {name}")
    if dry:
        return 0
    done = subprocess.run(["phais", "submit", bots[0], "--name", name], cwd=ROOT)
    if done.returncode != 0:
        sys.exit(f"submit: phais failed (exit {done.returncode}). The name was not used.")
    with open(USED, "a") as f:
        f.write(name + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
