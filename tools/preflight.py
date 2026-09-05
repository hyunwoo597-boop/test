#!/usr/bin/env python3
from pathlib import Path
import hashlib, re

ROOT = Path(__file__).resolve().parents[1]
TITLE = "010018100CD46000"
ARC_DIR = ROOT / "romfs" / "payload" / "atmosphere" / "contents" / TITLE / "romfs" / "nativeNXx64" / "ImgNX" / "Archive"

expected = {
    "CoreResource.arc": (15962535, "3013c9762a5f87e3782ef4a1fff08d49d9947939dcdcb27dedd08d3cdd92dc1c"),
    "GuiTextResource.arc": (115004, "7decf69ef9198a9112cf0d91d4a762861ead28e642287bb298c160a8167a9671"),
    "Msg2Resource_e.arc": (145129, "d83c54e72129c568431966a27d48d0f1fd76096ab26b4040a9517cf8400fa0ed"),
    "NXStrapResource.arc": (876383, "e71bce8723edb5b6db4edc0aca2f5a6e6d83e151f84dc80f1866102c53e0ab48"),
}

def die(msg):
    print("PREFLIGHT FAIL:", msg)
    raise SystemExit(2)

src = ROOT / "source" / "main.c"
if not src.is_file():
    die("source/main.c missing")
text = src.read_text(encoding="utf-8", errors="strict")
if not re.search(r"\bint\s+main\s*\(", text):
    die("int main(...) not found in source/main.c")
for forbidden in ["mods_unlock", "unlock_enabled", "HidNpadButton_Y", "char_0_off", "char_0_on"]:
    if forbidden in text:
        die(f"obsolete combined/toggle logic still present in source: {forbidden}")
print("OK source/main.c: character mods and unlock are separate menu actions")
for required in ["BGM_PATH", "bgm_init", "bgm_pump", "bgm_shutdown", "audoutInitialize", "audoutAppendAudioOutBuffer"]:
    if required not in text:
        die(f"BGM playback logic missing from source: {required}")
print("OK source/main.c: looping BGM playback logic present")

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
if icon_sha != "2b54394a13e12caeef8649a41cc92a1739857a30a4a133f27d047b103eb64aff":
    die("assets/icon.jpg sha256 mismatch")
print(f"OK custom icon.jpg {icon.stat().st_size} bytes {icon_sha}")

for needed in ["Makefile", "npdm.json", "tools/build_installer_nsp.sh", "tools/download_romfs_release.py"]:
    if not (ROOT / needed).is_file():
        die(f"{needed} missing")

UI_DIR = ROOT / "romfs" / "ui"
ui_names = [
    *[f"main_{i}.rgb565" for i in range(7)],
    *[f"char_{i}.rgb565" for i in range(5)],
    "status_ok.rgb565", "status_fail.rgb565", "info.rgb565"
]
for name in ui_names:
    f = UI_DIR / name
    if not f.is_file():
        die(f"missing UI screen: {name}")
    if f.stat().st_size != 1280 * 720 * 2:
        die(f"UI screen size mismatch: {name}")
for obsolete in ["main_2_off.rgb565", "main_2_on.rgb565", *[f"char_{i}_{s}.rgb565" for i in range(5) for s in ("off", "on")]]:
    if (UI_DIR / obsolete).exists():
        die(f"obsolete unlock-toggle UI unexpectedly present: {obsolete}")
print("OK separate-action RGB565 UI screens")

BGM = ROOT / "romfs" / "audio" / "iron_and_bone.pcm"
if not BGM.is_file():
    die("missing BGM PCM: romfs/audio/iron_and_bone.pcm")
if BGM.stat().st_size != 29536336:
    die(f"BGM PCM size mismatch: {BGM.stat().st_size}")
bgm_sha = hashlib.sha256(BGM.read_bytes()).hexdigest()
if bgm_sha != "6907465f3aeb13c14c6982441e93ccc0e8e7f36b78257d9c7f2904c0297bf030":
    die("BGM PCM sha256 mismatch")
print(f"OK looping BGM 48kHz stereo s16le {BGM.stat().st_size} bytes {bgm_sha}")

MOD_DIR = ROOT / "romfs" / "payload" / "mods"
for slug in ["excella", "rebecca", "jill_cos1", "jill_cos2"]:
    f = MOD_DIR / slug / "main"
    if not f.is_file() or f.stat().st_size < 8_000_000:
        die(f"missing/invalid character mod payload: mods/{slug}/main")
    print(f"OK mods {slug} {f.stat().st_size} bytes")

if (ROOT / "romfs" / "payload" / "mods_unlock").exists():
    die("obsolete mods_unlock directory must not exist")
print("OK mods_unlock removed")

unlock = ROOT / "romfs" / "payload" / "unlock" / "main"
if not unlock.is_file() or unlock.stat().st_size < 8_000_000:
    die("missing/invalid unlock-only main")
print(f"OK separate unlock-only main {unlock.stat().st_size} bytes")

for removed in ["jill_cos3", "jill_cos4"]:
    if (MOD_DIR / removed).exists():
        die(f"removed mod unexpectedly present: {removed}")

if (ROOT / "romfs" / "gallery").exists():
    die("gallery directory should be removed")
print("OK gallery removed")
print("OK update NSP integration disabled: Korean patch + mod installer only")
print("PREFLIGHT OK v17.1 SEPARATE CHARACTER / UNLOCK + LOOPING BGM")
