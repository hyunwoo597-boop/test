#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "BUILD FAIL at line $LINENO: $BASH_COMMAND" >&2' ERR

: "${PROD_KEYS:?Set PROD_KEYS to path of your prod.keys}"
TITLEID="0100F5A0C0DE0000"
TARGET="re5_korean_patch_installer"

test -s "$PROD_KEYS" || { echo "prod.keys missing/empty: $PROD_KEYS" >&2; exit 2; }
test -f source/main.c || { echo "source/main.c missing" >&2; exit 2; }
grep -Eq '\bint[[:space:]]+main[[:space:]]*\(' source/main.c || {
  echo "main() missing from source/main.c" >&2; exit 2;
}

python3 tools/preflight.py

echo "=== libnx Makefile source discovery ==="
make print-config

rm -rf build exefs control hacBrewPack-src hacbrewpack_nsp hacbrewpack_nca hacbrewpack_temp \
       RE5_Korean_Patch_Installer.nsp

echo "=== compile libnx application ==="
make -j2

for f in "${TARGET}.elf" "${TARGET}.nso" "${TARGET}.npdm" "${TARGET}.nsp"; do
  test -s "$f" || { echo "Expected build output missing: $f" >&2; exit 3; }
done

echo "=== verify linked application outputs ==="
# The linker has already resolved libnx crt0 -> main successfully if the ELF exists.
# Do not depend on an optional standalone nm binary that is not shipped in every
# devkitPro container image.
test -s "${TARGET}.elf" || { echo "ELF missing" >&2; exit 3; }
test -s "${TARGET}.nso" || { echo "NSO missing" >&2; exit 3; }
test -s "${TARGET}.npdm" || { echo "NPDM missing" >&2; exit 3; }
echo "ELF/NSO/NPDM OK; crt0 -> main link succeeded"

echo "=== create NACP/control ==="
nacptool --create \
  "RE5 Korean Patch Installer" \
  "RE5 Korean Patch Project" \
  "5.0.0" \
  "${TARGET}.nacp" \
  --titleid="$TITLEID"

test -s "${TARGET}.nacp" || { echo "NACP generation failed" >&2; exit 4; }

mkdir -p exefs control
cp "${TARGET}.nso" exefs/main
cp "${TARGET}.npdm" exefs/main.npdm
cp "${TARGET}.nacp" control/control.nacp

ICON="${DEVKITPRO}/libnx/default_icon.jpg"
test -s "$ICON" || { echo "libnx default icon missing: $ICON" >&2; exit 4; }
cp "$ICON" control/icon_AmericanEnglish.dat

echo "=== build hacBrewPack ==="
git clone --depth=1 https://github.com/pplatoon/hacBrewPack.git hacBrewPack-src
cp hacBrewPack-src/config.mk.template hacBrewPack-src/config.mk
make -C hacBrewPack-src -j2
test -x hacBrewPack-src/hacbrewpack || { echo "hacbrewpack build failed" >&2; exit 5; }

echo "=== package installer NSP ==="
echo "hacBrewPack reads TitleID from control/control.nacp."
./hacBrewPack-src/hacbrewpack \
  --keyset "$PROD_KEYS" \
  --exefsdir exefs \
  --romfsdir romfs \
  --controldir control \
  --nologo

INSTALLER_NSP="$(find hacbrewpack_nsp -maxdepth 2 -type f -name '*.nsp' -print -quit)"
test -n "$INSTALLER_NSP" && test -s "$INSTALLER_NSP" || {
  echo "hacBrewPack did not produce an NSP" >&2
  find . -maxdepth 3 -type f | sort
  exit 6
}
cp "$INSTALLER_NSP" RE5_Korean_Patch_Installer.nsp
test -s RE5_Korean_Patch_Installer.nsp

echo "Installer NSP built: $(stat -c %s RE5_Korean_Patch_Installer.nsp) bytes"
