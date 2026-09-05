#!/usr/bin/env python3
from __future__ import annotations
import json
import os
import sys
import urllib.request
import zipfile
from pathlib import Path

ASSET_NAME = "RE5_v17_romfs_payload.zip"
ROOT = Path(__file__).resolve().parents[1]
TMP = ROOT / "input" / ASSET_NAME
DEST = ROOT

def request_json(url: str):
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "RE5-v17-Payload-GitHub-Actions",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)

def download(url: str, out: Path):
    headers = {"User-Agent": "RE5-v17-Payload-GitHub-Actions"}
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    out.parent.mkdir(parents=True, exist_ok=True)
    part = out.with_suffix(out.suffix + ".part")
    with urllib.request.urlopen(req, timeout=300) as r, part.open("wb") as f:
        while True:
            chunk = r.read(1024 * 1024)
            if not chunk:
                break
            f.write(chunk)
    part.replace(out)

def safe_extract(zpath: Path, dest: Path):
    with zipfile.ZipFile(zpath, "r") as z:
        names = z.namelist()
        if not names:
            raise RuntimeError("payload ZIP is empty")
        for name in names:
            clean = name.replace("\\", "/")
            if clean.startswith("/") or ".." in Path(clean).parts:
                raise RuntimeError(f"unsafe ZIP path: {name}")
            if not (clean == "romfs/payload" or clean.startswith("romfs/payload/")):
                raise RuntimeError(f"unexpected ZIP entry outside romfs/payload: {name}")
        z.extractall(dest)

repo = os.environ.get("GITHUB_REPOSITORY", "").strip()
if not repo or "/" not in repo:
    print("ERROR: GITHUB_REPOSITORY is missing.", file=sys.stderr)
    sys.exit(2)

api = f"https://api.github.com/repos/{repo}/releases/latest"
print(f"Reading latest release: {api}")
release = request_json(api)
asset = next((a for a in release.get("assets", []) if a.get("name") == ASSET_NAME), None)
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
download(url, TMP)
if TMP.stat().st_size < 10_000_000:
    print(f"ERROR: payload ZIP is unexpectedly small: {TMP.stat().st_size} bytes", file=sys.stderr)
    sys.exit(5)

print("Extracting payload into repository root...")
safe_extract(TMP, DEST)
required = ROOT / "romfs" / "payload" / "atmosphere"
if not required.is_dir():
    print("ERROR: extracted payload is incomplete: romfs/payload/atmosphere missing", file=sys.stderr)
    sys.exit(6)
print(f"OK: payload extracted from {ASSET_NAME} ({TMP.stat().st_size:,} bytes)")
