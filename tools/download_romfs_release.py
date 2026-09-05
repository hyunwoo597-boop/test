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

BASE_ASSET_NAME = "RE5_v17_FULL_ROMFS.zip"
BASE_EXPECTED_SHA256 = "3871f2c56d5b9473f4404a7cb4aef7c1766a864a641f81f9b5e8b600efee7e31"
PATCH_ASSET_NAME = "RE5_v17_ROMFS_PATCH_17_1.zip"
PATCH_EXPECTED_SHA256 = "2c8d640c9c926f13b09c91701d5684898a2929b120016e2addc0e5cedb4beeeb"
ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "input"


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


def verify_sha(path: Path, expected: str, label: str):
    sha = hashlib.sha256(path.read_bytes()).hexdigest()
    if sha != expected:
        raise RuntimeError(f"{label} sha256 mismatch: {sha}")
    print(f"{label} SHA256 OK: {sha}")


def validate_zip_paths(zpath: Path):
    with zipfile.ZipFile(zpath, "r") as z:
        names = z.namelist()
        if not names:
            raise RuntimeError(f"ZIP is empty: {zpath.name}")
        for name in names:
            clean = name.replace("\\", "/")
            parts = Path(clean).parts
            if clean.startswith("/") or ".." in parts:
                raise RuntimeError(f"unsafe ZIP path: {name}")
            if not (clean == "romfs" or clean.startswith("romfs/")):
                raise RuntimeError(f"unexpected ZIP entry outside romfs/: {name}")


def extract_base(zpath: Path):
    romfs = ROOT / "romfs"
    if romfs.exists():
        shutil.rmtree(romfs)
    validate_zip_paths(zpath)
    with zipfile.ZipFile(zpath, "r") as z:
        z.extractall(ROOT)


def overlay_patch(zpath: Path):
    validate_zip_paths(zpath)
    with zipfile.ZipFile(zpath, "r") as z:
        z.extractall(ROOT)


def get_asset(release: dict, name: str):
    asset = next((a for a in release.get("assets", []) if a.get("name") == name), None)
    if not asset:
        raise RuntimeError(
            f"latest release has no asset named exactly: {name}; "
            f"assets found: {[a.get('name') for a in release.get('assets', [])]}"
        )
    url = asset.get("browser_download_url")
    if not url:
        raise RuntimeError(f"release asset has no browser_download_url: {name}")
    return url


repo = os.environ.get("GITHUB_REPOSITORY", "").strip()
if not repo or "/" not in repo:
    print("ERROR: GITHUB_REPOSITORY is missing.", file=sys.stderr)
    sys.exit(2)

try:
    api = f"https://api.github.com/repos/{repo}/releases/latest"
    print(f"Reading latest release: {api}")
    release = request_json(api)

    base_path = INPUT / BASE_ASSET_NAME
    patch_path = INPUT / PATCH_ASSET_NAME

    print("Downloading base ROMFS:", BASE_ASSET_NAME)
    download(get_asset(release, BASE_ASSET_NAME), base_path)
    if base_path.stat().st_size < 50_000_000:
        raise RuntimeError(f"base ROMFS ZIP unexpectedly small: {base_path.stat().st_size} bytes")
    verify_sha(base_path, BASE_EXPECTED_SHA256, "Base ROMFS")

    print("Downloading ROMFS overlay patch:", PATCH_ASSET_NAME)
    download(get_asset(release, PATCH_ASSET_NAME), patch_path)
    if patch_path.stat().st_size < 1_000_000:
        raise RuntimeError(f"ROMFS patch ZIP unexpectedly small: {patch_path.stat().st_size} bytes")
    verify_sha(patch_path, PATCH_EXPECTED_SHA256, "ROMFS patch")

    print("Extracting base romfs/ ...")
    extract_base(base_path)
    print("Overlaying changed ROMFS files only ...")
    overlay_patch(patch_path)

    for needed in [ROOT / "romfs" / "payload", ROOT / "romfs" / "ui", ROOT / "romfs" / "audio"]:
        if not needed.is_dir():
            raise RuntimeError(f"final ROMFS incomplete: {needed} missing")

    print(
        f"OK: base {BASE_ASSET_NAME} + patch {PATCH_ASSET_NAME} ready "
        f"({base_path.stat().st_size:,} + {patch_path.stat().st_size:,} bytes downloaded)"
    )
except Exception as e:
    print("ERROR:", e, file=sys.stderr)
    sys.exit(3)
