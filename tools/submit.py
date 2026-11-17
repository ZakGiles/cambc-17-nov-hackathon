#!/usr/bin/env python3
"""Submit a bot under the next unused name in names.txt.

  tools/submit.py main          # phais submit main --name "<next name>"
  tools/submit.py main --dry    # print the next name only

names.txt holds one name per line, top first. Blank lines are skipped.
Each line is used once. names.used holds the used line numbers. Before
choosing, the script reads names.used on origin/main too, since another
clone may have submitted. After a submit it pushes names.used to main.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = os.path.join(ROOT, "names.txt")
USED = os.path.join(ROOT, "names.used")


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)


def numbered():
    if not os.path.exists(NAMES):
        return []
    with open(NAMES) as f:
        return [(i, s.strip()) for i, s in enumerate(f, 1) if s.strip()]


def parse(text):
    return {int(s) for s in text.split() if s.isdigit()}


def local_used():
    if not os.path.exists(USED):
        return set()
    with open(USED) as f:
        return parse(f.read())


def main_used():
    # Lines another clone used and pushed to main.
    if git("fetch", "-q", "origin", "main").returncode != 0:
        print("submit: git fetch failed. Using the local names.used only.")
        return set()
    r = git("show", "origin/main:names.used")
    return parse(r.stdout) if r.returncode == 0 else set()


def write_used(nums):
    with open(USED, "w") as f:
        f.write("".join(f"{k}\n" for k in sorted(nums)))


def push_used(nums, n):
    # Drop the local edit so the pull cannot conflict on names.used. Then merge.
    git("checkout", "--", "names.used")
    r = git("pull", "-q", "--rebase", "--autostash")
    nums = nums | local_used()
    write_used(nums)
    if r.returncode != 0:
        print(f"submit: git pull failed. Push names.used by hand.\n{(r.stdout + r.stderr).strip()}")
        return
    if git("diff", "--quiet", "HEAD", "--", "names.used").returncode == 0:
        print("names.used already up to date on main")
        return
    for step in (["add", "names.used"],
                 ["commit", "-q", "-m", f"names.used: line {n}", "--", "names.used"],
                 ["push", "-q", "origin", "HEAD:main"]):
        r = git(*step)
        if r.returncode != 0:
            print(f"submit: git {step[0]} failed. Push names.used by hand.\n{(r.stdout + r.stderr).strip()}")
            return
    print("names.used pushed to main")


def main():
    dry = "--dry" in sys.argv[1:]
    bots = [a for a in sys.argv[1:] if a != "--dry"]
    if len(bots) != 1:
        sys.exit(__doc__)
    done_lines = local_used() | main_used()
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
    nums = done_lines | {n}
    write_used(nums)
    push_used(nums, n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
