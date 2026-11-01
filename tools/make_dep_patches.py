"""Writes our changes to the bundled libraries as patch files (deps/patches/<name>.patch).

Each library in indy64recomp/lib is compared with the original at its pinned commit (the GitHub zip in
third_party/_dl, see deps/deps.json). Only the library's own files are compared; its git submodules (from
.gitmodules) are left out, they are fetched unchanged. Needs git (for "git diff --no-index").

usage: make_dep_patches.py <git.exe>
"""
import configparser
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEPS = os.path.join(ROOT, "deps")
CACHE = os.path.join(ROOT, "third_party", "_dl")
SKIP_NAMES = {".git", "__pycache__"}


def submodule_paths(tree):
    gm = os.path.join(tree, ".gitmodules")
    if not os.path.exists(gm):
        return set()
    cfg = configparser.ConfigParser()
    cfg.read_string(open(gm, encoding="utf-8").read())
    return {cfg[s]["path"].strip().replace("\\", "/") for s in cfg.sections() if "path" in cfg[s]}


def copy_own_files(src, dst, skip):
    for dirpath, dirnames, filenames in os.walk(src):
        rel = os.path.relpath(dirpath, src).replace("\\", "/")
        rel = "" if rel == "." else rel
        dirnames[:] = [d for d in dirnames if d not in SKIP_NAMES and (rel + "/" + d).strip("/") not in skip]
        for f in filenames:
            out = os.path.join(dst, rel, f)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            shutil.copy2(os.path.join(dirpath, f), out)


def extract_zip(zip_path, dst):
    with zipfile.ZipFile(zip_path) as z:
        prefix = z.namelist()[0]
        for info in z.infolist():
            name = info.filename[len(prefix):]
            if not name or info.is_dir():
                continue
            out = os.path.join(dst, name)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with z.open(info) as fsrc, open(out, "wb") as fdst:
                shutil.copyfileobj(fsrc, fdst)


def main():
    git = sys.argv[1]
    deps = json.load(open(os.path.join(DEPS, "deps.json"), encoding="utf-8"))
    os.makedirs(os.path.join(DEPS, "patches"), exist_ok=True)
    for dep in deps["libraries"]:
        lib = os.path.join(ROOT, "indy64recomp", *dep["path"].split("/"))
        zip_path = os.path.join(CACHE, dep["cache_zip"])
        with tempfile.TemporaryDirectory(dir="D:\\") as tmp:
            a, b = os.path.join(tmp, "a"), os.path.join(tmp, "b")
            base_full = os.path.join(tmp, "base")
            extract_zip(zip_path, base_full)
            skip = submodule_paths(base_full)
            copy_own_files(base_full, a, skip)
            copy_own_files(lib, b, skip)
            for extra in dep.get("ignore", []):
                shutil.rmtree(os.path.join(b, *extra.split("/")), ignore_errors=True)
                p = os.path.join(b, *extra.split("/"))
                if os.path.isfile(p):
                    os.remove(p)
            out = subprocess.run([git, "-c", "core.autocrlf=false", "diff", "--no-index", "--binary", "--no-color",
                                  "--no-prefix", "a", "b"], cwd=tmp, capture_output=True)
            patch = out.stdout.decode("utf-8", errors="replace")
            path = os.path.join(DEPS, "patches", dep["name"] + ".patch")
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(patch)
            files = patch.count("\ndiff --git ") + (1 if patch.startswith("diff --git ") else 0)
            print(f"{dep['name']:20} {files:4} files changed -> {os.path.relpath(path, ROOT)} ({len(patch):,} bytes)")


main()
