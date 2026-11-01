"""Download a GitHub repository at a commit together with all its submodules (recursively), without git.

Each repository is fetched as a zip from codeload.github.com and extracted with Windows' tar via a short temp
folder (some repositories exceed MAX_PATH). Submodule commits come from the GitHub tree API, their URLs from
.gitmodules. Zips are cached in third_party/_dl.

usage: fetch_tree.py <owner/repo> <commit> <target dir> [--skip path ...] [--exclude tar-pattern ...]
  --skip     submodule paths (relative to the top repository) not to fetch, e.g. lib/hm64-decomp
  --exclude  tar exclude patterns applied to every extraction, e.g. */shaders* for SPIRV-Cross tests
Set GITHUB_TOKEN to raise the API rate limit.
"""
import argparse
import configparser
import io
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "third_party", "_dl")


def api(url):
    req = urllib.request.Request(url, headers={"User-Agent": "indy64"})
    if os.environ.get("GITHUB_TOKEN"):
        req.add_header("Authorization", "Bearer " + os.environ["GITHUB_TOKEN"])
    with urllib.request.urlopen(req) as r:
        return r.read()


def repo_from_url(url):
    m = re.search(r"github\.com[:/]([^/]+)/([^/\s]+?)(?:\.git)?/?$", url.strip())
    if not m:
        raise ValueError("not a GitHub URL: " + url)
    return f"{m.group(1)}/{m.group(2)}"


def extract(repo, commit, target, excludes):
    os.makedirs(CACHE, exist_ok=True)
    zip_path = os.path.join(CACHE, f"{repo.replace('/', '_')}_{commit[:7]}.zip")
    if not os.path.exists(zip_path):
        data = api(f"https://codeload.github.com/{repo}/zip/{commit}")
        with open(zip_path, "wb") as f:
            f.write(data)
    tmp = "D:\\_x_" + commit[:7]
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    cmd = ["tar.exe", "-xf", zip_path, "-C", tmp]
    for e in excludes:
        cmd += ["--exclude", e]
    subprocess.run(cmd, check=True)
    inner = os.path.join(tmp, os.listdir(tmp)[0])
    if os.path.exists(target):
        shutil.rmtree(target)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    shutil.move(inner, target)
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"{repo:50} {commit[:10]} {os.path.getsize(zip_path):>12,} -> {target}", flush=True)


def fetch(repo, commit, target, skip, excludes, prefix=""):
    extract(repo, commit, target, excludes)
    gm = os.path.join(target, ".gitmodules")
    if not os.path.exists(gm):
        return
    cfg = configparser.ConfigParser()
    cfg.read_string(open(gm, encoding="utf-8").read())
    urls = {cfg[s]["path"].strip(): cfg[s]["url"].strip() for s in cfg.sections() if "path" in cfg[s]}
    tree = json.loads(api(f"https://api.github.com/repos/{repo}/git/trees/{commit}?recursive=1"))
    for entry in tree["tree"]:
        if entry["type"] != "commit":
            continue
        path = entry["path"]
        full = (prefix + "/" + path).strip("/")
        if full in skip:
            print(f"{'(skipped)':50} {full}", flush=True)
            continue
        if path not in urls:
            print(f"warning: no URL for submodule {full}", file=sys.stderr)
            continue
        fetch(repo_from_url(urls[path]), entry["sha"], os.path.join(target, *path.split("/")), skip, excludes, full)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("repo")
    ap.add_argument("commit")
    ap.add_argument("target")
    ap.add_argument("--skip", nargs="*", default=[])
    ap.add_argument("--exclude", nargs="*", default=[])
    a = ap.parse_args()
    fetch(a.repo, a.commit, os.path.abspath(a.target), set(a.skip), a.exclude)


if __name__ == "__main__":
    main()
