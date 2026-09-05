#!/usr/bin/env python3
from __future__ import annotations
import hashlib
import json
import os
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path

ASSET_NAME = "RE5_v17_FULL_ROMFS.zip"
EXPECTED_SHA256 = "3871f2c56d5b9473f4404a7cb4aef7c1766a864a641f81f9b5e8b600efee7e31"
ROOT = Path(__file__).resolve().parents[1]
TMP = ROOT / "input" / ASSET_NAME
DEST = ROOT

def request_json(url: str):
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "RE5-v17-ROMFS-GitHub-Actions",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)

def download(url: str, out: Path):
    headers = {"User-Agent": "RE5-v17-ROMFS-GitHub-Actions"}
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    out.parent.mkdir(parents=True, exist_ok=True)
    part = out.with_suffix(out.suffix + ".part")
    with urllib.request.urlopen(req, timeout=600) as r, part.open("wb") as f:
        while True:
            chunk = r.read(1024 * 1024)
            if not chunk:
                break
            f.write(chunk)
    part.replace(out)

def safe_extract_romfs(zpath: Path, dest: Path):
    romfs = dest / "romfs"
    if romfs.exists():
        shutil.rmtree(romfs)
    with zipfile.ZipFile(zpath, "r") as z:
        names = z.namelist()
        if not names:
            raise RuntimeError("ROMFS ZIP is empty")
        for name in names:
            clean = name.replace("\\", "/")
            parts = Path(clean).parts
            if clean.startswith("/") or ".." in parts:
                raise RuntimeError(f"unsafe ZIP path: {name}")
            if not (clean == "romfs" or clean.startswith("romfs/")):
                raise RuntimeError(f"unexpected ZIP entry outside romfs/: {name}")
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
if TMP.stat().st_size < 50_000_000:
    print(f"ERROR: ROMFS ZIP is unexpectedly small: {TMP.stat().st_size} bytes", file=sys.stderr)
    sys.exit(5)

sha = hashlib.sha256(TMP.read_bytes()).hexdigest()
if sha != EXPECTED_SHA256:
    print(f"ERROR: ROMFS ZIP sha256 mismatch: {sha}", file=sys.stderr)
    sys.exit(6)
print("ROMFS ZIP SHA256 OK:", sha)

print("Extracting complete romfs/ ...")
safe_extract_romfs(TMP, DEST)
for needed in [ROOT / "romfs" / "payload", ROOT / "romfs" / "ui"]:
    if not needed.is_dir():
        print(f"ERROR: extracted ROMFS incomplete: {needed} missing", file=sys.stderr)
        sys.exit(7)
print(f"OK: complete romfs extracted from {ASSET_NAME} ({TMP.stat().st_size:,} bytes)")
