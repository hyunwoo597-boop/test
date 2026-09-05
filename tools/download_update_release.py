#!/usr/bin/env python3
from __future__ import annotations
import json
import os
import sys
import urllib.request
from pathlib import Path

ASSET_NAME = "RE5_Update_1.11.nsp"
OUT = Path("input") / ASSET_NAME

def request_json(url: str):
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "RE5-AllInOne-GitHub-Actions",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)

def download(url: str, out: Path):
    headers = {"User-Agent": "RE5-AllInOne-GitHub-Actions"}
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix(out.suffix + ".part")
    with urllib.request.urlopen(req, timeout=120) as r, tmp.open("wb") as f:
        while True:
            chunk = r.read(1024 * 1024)
            if not chunk:
                break
            f.write(chunk)
    tmp.replace(out)

repo = os.environ.get("GITHUB_REPOSITORY", "").strip()
if not repo or "/" not in repo:
    print("ERROR: GITHUB_REPOSITORY is missing.", file=sys.stderr)
    sys.exit(2)

api = f"https://api.github.com/repos/{repo}/releases/latest"
print(f"Reading latest release: {api}")
release = request_json(api)

asset = None
for a in release.get("assets", []):
    if a.get("name") == ASSET_NAME:
        asset = a
        break

if not asset:
    print(f"ERROR: latest release has no asset named exactly: {ASSET_NAME}", file=sys.stderr)
    print("Release:", release.get("html_url", "(unknown)"), file=sys.stderr)
    print("Assets found:", [a.get("name") for a in release.get("assets", [])], file=sys.stderr)
    sys.exit(3)

url = asset.get("browser_download_url")
if not url:
    print("ERROR: release asset has no browser_download_url.", file=sys.stderr)
    sys.exit(4)

print("Downloading:", url)
download(url, OUT)

size = OUT.stat().st_size
if size < 50_000_000:
    print(f"ERROR: downloaded file is unexpectedly small: {size} bytes", file=sys.stderr)
    sys.exit(5)

with OUT.open("rb") as f:
    if f.read(4) != b"PFS0":
        print("ERROR: downloaded asset is not a PFS0 NSP.", file=sys.stderr)
        sys.exit(6)

print(f"OK: {OUT} ({size:,} bytes)")
