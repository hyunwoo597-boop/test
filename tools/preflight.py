#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, struct, re

ROOT = Path(__file__).resolve().parents[1]
TITLE = "010018100CD46000"
ARC_DIR = ROOT / "romfs" / "payload" / "atmosphere" / "contents" / TITLE / "romfs" / "nativeNXx64" / "ImgNX" / "Archive"

expected = {
    "CoreResource.arc": (15960334, "a14416f05f3d7c83561a9d09ae0bd07565aab09da5a9f19ef5a2a1d8d253c174"),
    "GuiTextResource.arc": (115004, "7decf69ef9198a9112cf0d91d4a762861ead28e642287bb298c160a8167a9671"),
    "Msg2Resource_e.arc": (145129, "25c49f1bc704a40216c297a5d8a186565a7b5adf35bbc74841f4bbb51e30a755"),
}

ap = argparse.ArgumentParser()
ap.add_argument("--require-update", action="store_true")
args = ap.parse_args()

def die(msg):
    print("PREFLIGHT FAIL:", msg)
    raise SystemExit(2)

src = ROOT / "source" / "main.c"
if not src.is_file():
    die("source/main.c missing")
text = src.read_text(encoding="utf-8", errors="strict")
if not re.search(r"\bint\s+main\s*\(", text):
    die("int main(...) not found in source/main.c")
print("OK source/main.c contains main()")

for name, (size, sha) in expected.items():
    p = ARC_DIR / name
    if not p.is_file():
        die(f"missing payload: {name}")
    actual_size = p.stat().st_size
    actual_sha = hashlib.sha256(p.read_bytes()).hexdigest()
    if actual_size != size:
        die(f"{name}: size {actual_size} != {size}")
    if actual_sha != sha:
        die(f"{name}: sha256 mismatch")
    print(f"OK {name} {actual_size} {actual_sha}")

# v14 validated additions
nxstrap = ARC_DIR / "NXStrapResource.arc"
if not nxstrap.is_file():
    die("missing payload: NXStrapResource.arc")
if nxstrap.stat().st_size != 876383:
    die("NXStrapResource.arc size mismatch")
nxsha = hashlib.sha256(nxstrap.read_bytes()).hexdigest()
if nxsha != "e71bce8723edb5b6db4edc0aca2f5a6e6d83e151f84dc80f1866102c53e0ab48":
    die("NXStrapResource.arc sha256 mismatch")
print(f"OK NXStrapResource.arc {nxstrap.stat().st_size} {nxsha}")

ips = ROOT / "romfs" / "payload" / "atmosphere" / "exefs_patches" / "RE5_Manual_CrashFix" / "C517ECBB79DE97338E98147A6B5B5F2B.ips"
if not ips.is_file():
    die("missing MANUAL CrashFix IPS")
ipsha = hashlib.sha256(ips.read_bytes()).hexdigest()
if ipsha != "db3395eb48bb5661e5706cf1a439df2c7ecb7f8619fc708d22c87d5dcfcb6496":
    die("MANUAL CrashFix IPS sha256 mismatch")
print(f"OK MANUAL CrashFix IPS {ips.stat().st_size} {ipsha}")

icon = ROOT / "assets" / "icon.jpg"
if not icon.is_file() or icon.stat().st_size < 1024:
    die("assets/icon.jpg missing/invalid")
print(f"OK custom icon.jpg {icon.stat().st_size} bytes")

for needed in ["Makefile", "npdm.json", "tools/build_installer_nsp.sh", "tools/merge_nsp.py", "tools/download_update_release.py"]:
    if not (ROOT / needed).is_file():
        die(f"{needed} missing")

nsp = ROOT / "input" / "RE5_Update_1.11.nsp"
if nsp.is_file():
    with nsp.open("rb") as f:
        hdr = f.read(0x10)
    if hdr[:4] != b"PFS0":
        die("update NSP does not begin with PFS0")
    count, strsize = struct.unpack_from("<II", hdr, 4)
    if count < 3:
        die(f"unexpected update NSP file count: {count}")
    print(f"OK update NSP PFS0 file_count={count}, size={nsp.stat().st_size}")
elif args.require_update:
    die("input/RE5_Update_1.11.nsp missing after release download")
else:
    print("OK update NSP intentionally absent before Release Asset download")

print("PREFLIGHT OK")
