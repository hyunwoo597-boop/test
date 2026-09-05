RE5 v17 Korean Patch + Character Mod Installer - GitHub build setup
SEPARATE CHARACTER / UNLOCK edition

This repository contains NO romfs directory.

Upload exactly ONE asset to the latest GitHub Release:
  RE5_v17_FULL_ROMFS.zip
  SHA256: 3871f2c56d5b9473f4404a7cb4aef7c1766a864a641f81f9b5e8b600efee7e31

DO NOT upload or use RE5_Update_1.11.nsp for this build.
This project builds only the Korean patch + character mod/unlock installer NSP.

Repository secret required:
  PROD_KEYS_B64

Run:
  Actions -> Build RE5 Korean + Mod Installer NSP -> Run workflow

Expected artifact:
  RE5_Korean_Mod_Installer.nsp
  SHA256SUMS.txt

Character mods and full unlock are separate menu actions.
The old mods_unlock combined payload and Y-button unlock toggle are removed.
Because both features replace the same exefs/main, the last installed one is active.
Gallery resources/functionality are removed.
