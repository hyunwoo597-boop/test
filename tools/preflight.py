#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, struct, re

ROOT = Path(__file__).resolve().parents[1]
TITLE = "010018100CD46000"
ARC_DIR = ROOT / "romfs" / "payload" / "atmosphere" / "contents" / TITLE / "romfs" / "nativeNXx64" / "ImgNX" / "Archive"

expected = {
    "CoreResource.arc": (15962535, "3013c9762a5f87e3782ef4a1fff08d49d9947939dcdcb27dedd08d3cdd92dc1c"),
    "GuiTextResource.arc": (115004, "7decf69ef9198a9112cf0d91d4a762861ead28e642287bb298c160a8167a9671"),
    "Msg2Resource_e.arc": (145129, "d83c54e72129c568431966a27d48d0f1fd76096ab26b4040a9517cf8400fa0ed"),
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
icon_sha = hashlib.sha256(icon.read_bytes()).hexdigest()
if icon_sha != "3dc4158938357a8559841b79e0b3631621fb24183d4b3ad41a0d2e87c2003b3c":
    die("assets/icon.jpg sha256 mismatch")
print(f"OK custom icon.jpg {icon.stat().st_size} bytes {icon_sha}")

# v15 launch gallery
GALLERY_DIR = ROOT / "romfs" / "gallery"
gallery_hashes = {
    "page1.rgb565": "091cd611f5f3dc33ac4ad771a74b183cf683f7bba0ad56f46e9d87e9814315d8",
    "page2.rgb565": "69ecac12b30fb6702fb97c5bef1232cd9b00b55dbd1c43f646f18f537d5e53eb",
    "page3.rgb565": "ab46177d33838d102bdfd2ae76c69b0b0965bb72bb0ada5fee9141c751f43bfc",
    "page4.rgb565": "85326a7f4d00064854a09b873baee531aea3c51ec39f6c6295f18c18acf9d0d1",
    "page5.rgb565": "c19ca31937746b57019f53aff6b77a51b87823dd53b0c2c14fd91b538af73ffe",
    "page6.rgb565": "ad2a07728bc9069c9a72b36ef65278221f1ca42b5730eefedc93ab25772dda17",
    "page7.rgb565": "9014a2a96feee94ffc8ec77c5540fd0ad7063040dc257bf2e429b5f9b1b5f54d",
    "page8.rgb565": "cbb8b01314cd295f31039b512e7c9716b5c832b3f3790d2b688368a913dcfca6"
}
for name, expected_sha in gallery_hashes.items():
    f = GALLERY_DIR / name
    if not f.is_file():
        die(f"missing gallery image: {name}")
    if f.stat().st_size != 1280 * 720 * 2:
        die(f"gallery image size mismatch: {name}")
    got = hashlib.sha256(f.read_bytes()).hexdigest()
    if got != expected_sha:
        die(f"gallery image sha256 mismatch: {name}")
    print(f"OK gallery {name} {got}")

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


# v17 preview-style Korean UI + 4 character mods + unlock combinations
UI_DIR = ROOT / "romfs" / "ui"
ui_names = [
    *[f"main_{i}.rgb565" for i in range(7)],
    "main_2_off.rgb565", "main_2_on.rgb565",
    *[f"char_{i}_{state}.rgb565" for i in range(5) for state in ("off","on")],
    "status_ok.rgb565", "status_fail.rgb565", "info.rgb565"
]
for name in ui_names:
    f = UI_DIR / name
    if not f.is_file():
        die(f"missing UI screen: {name}")
    if f.stat().st_size != 1280 * 720 * 2:
        die(f"UI screen size mismatch: {name}")
print("OK v17 preview-style RGB565 UI screens")

MOD_DIR = ROOT / "romfs" / "payload" / "mods"
MOD_UNLOCK_DIR = ROOT / "romfs" / "payload" / "mods_unlock"
for slug in ["excella", "rebecca", "jill_cos1", "jill_cos2"]:
    for parent in (MOD_DIR, MOD_UNLOCK_DIR):
        f = parent / slug / "main"
        if not f.is_file() or f.stat().st_size < 8_000_000:
            die(f"missing/invalid character mod payload: {parent.name}/{slug}/main")
        print(f"OK {parent.name} {slug} {f.stat().st_size} bytes")

unlock = ROOT / "romfs" / "payload" / "unlock" / "main"
if not unlock.is_file() or unlock.stat().st_size < 8_000_000:
    die("missing/invalid unlock-only main")
print(f"OK unlock-only main {unlock.stat().st_size} bytes")

for removed in ["jill_cos3","jill_cos4"]:
    if (MOD_DIR / removed).exists():
        die(f"removed mod unexpectedly present: {removed}")

print("PREFLIGHT OK v17")
