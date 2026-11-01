"""Sets up the libraries in indy64recomp/lib: downloads each one at its pinned commit with all submodules
(tools/fetch_tree.py, no git clone needed) and applies our changes from deps/patches (git apply).

usage: python tools/setup_deps.py [--target <dir>] [--git <git.exe>]
  --target  folder that receives lib/ (default: indy64recomp)
  --git     git executable for "git apply" (default: git on PATH)
Set GITHUB_TOKEN if GitHub's API rate limit (60 requests per hour without a token) is reached.
"""
import argparse
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import fetch_tree  # noqa: E402

# Same exclusions as for the original setup: zstd's tests contain symbolic links that tar cannot create on Windows.
EXCLUDES = ["zstd-*/tests*"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default=os.path.join(ROOT, "indy64recomp"))
    ap.add_argument("--git", default="git")
    a = ap.parse_args()
    deps = json.load(open(os.path.join(ROOT, "deps", "deps.json"), encoding="utf-8"))["libraries"]

    for dep in deps:
        if dep.get("submodule_of"):
            continue
        target = os.path.join(os.path.abspath(a.target), *dep["path"].split("/"))
        fetch_tree.fetch(dep["repo"], dep["commit"], target, set(), EXCLUDES)

    # Parents first (list order), so a submodule's patch applies to the fetched submodule.
    for dep in deps:
        target = os.path.join(os.path.abspath(a.target), *dep["path"].split("/"))
        patch = os.path.join(ROOT, "deps", "patches", dep["name"] + ".patch")
        if os.path.getsize(patch) == 0:
            continue
        subprocess.run([a.git, "-c", "core.autocrlf=false", "apply", "-p1", "--whitespace=nowarn", patch], cwd=target,
                       check=True)
        print(f"applied {os.path.relpath(patch, ROOT)} to {dep['path']}")


if __name__ == "__main__":
    main()
