#!/usr/bin/env python3
"""Submit a bot under the next unused name in names.txt.

  tools/submit.py main          # phais submit main --name "<next name>"
  tools/submit.py main --dry    # print the next name only

names.txt holds one name per line, top first. Blank lines are skipped.
Each line is used once. names.used holds the used line numbers. After a
submit it is committed and pushed to main.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = os.path.join(ROOT, "names.txt")
USED = os.path.join(ROOT, "names.used")


def numbered():
    if not os.path.exists(NAMES):
        return []
    with open(NAMES) as f:
        return [(i, s.strip()) for i, s in enumerate(f, 1) if s.strip()]


def used():
    if not os.path.exists(USED):
        return set()
    with open(USED) as f:
        return {int(s) for s in f if s.strip().isdigit()}


def push_used(n):
    for step in (["add", "names.used"],
                 ["commit", "-q", "-m", f"names.used: line {n}", "--", "names.used"],
                 ["pull", "-q", "--rebase", "--autostash"],
                 ["push", "-q", "origin", "HEAD:main"]):
        r = subprocess.run(["git", *step], cwd=ROOT, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"submit: git {step[0]} failed. Push names.used by hand.\n{(r.stdout + r.stderr).strip()}")
            return
    print("names.used pushed to main")


def main():
    dry = "--dry" in sys.argv[1:]
    bots = [a for a in sys.argv[1:] if a != "--dry"]
    if len(bots) != 1:
        sys.exit(__doc__)
    done_lines = used()
    n, name = next(((i, s) for i, s in numbered() if i not in done_lines), (None, None))
    if name is None:
        sys.exit(f"submit: no unused names left in {NAMES}")
    # The server takes names of 1 to 48 characters.
    name = name[:48].strip()
    print(f"line {n}: {name}")
    if dry:
        return 0
    done = subprocess.run(["phais", "submit", bots[0], "--name", name], cwd=ROOT)
    if done.returncode != 0:
        sys.exit(f"submit: phais failed (exit {done.returncode}). The name was not used.")
    with open(USED, "a") as f:
        f.write(f"{n}\n")
    push_used(n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
