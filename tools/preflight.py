from pathlib import Path
import hashlib, json, struct, zlib, sys
ROOT=Path(__file__).resolve().parents[1]

def die(s): print('FAIL:',s); sys.exit(1)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

# Exact patch source checks
expected={
 'CoreResource.arc': (15960334,'a14416f05f3d7c83561a9d09ae0bd07565aab09da5a9f19ef5a2a1d8d253c174'),
 'GuiTextResource.arc': (115004,'7decf69ef9198a9112cf0d91d4a762861ead28e642287bb298c160a8167a9671'),
 'Msg2Resource_e.arc': (145129,'e353214ba3e4e28ad45ddfbfaa889ca9f37da748184183ecfdc6948d0aac21cf'),
}
arcroot=ROOT/'romfs/payload/atmosphere/contents/010018100CD46000/romfs/nativeNXx64/ImgNX/Archive'
for name,(size,h) in expected.items():
 p=arcroot/name
 if not p.is_file() or p.stat().st_size!=size or sha(p)!=h: die(f'{name} mismatch')
 print('OK',name,size,h)

# Verify the exact two corrected glyph tokens in mes_system_e
p=arcroot/'Msg2Resource_e.arc'; b=p.read_bytes(); count=struct.unpack_from('<H',b,6)[0]; dec=None
for i in range(count):
 e=8+i*0x50; name=b[e:e+64].split(b'\0',1)[0].decode('ascii','replace')
 if name=='etc\\message\\mes_system_e':
  cs=struct.unpack_from('<I',b,e+68)[0]; off=struct.unpack_from('<I',b,e+76)[0]; dec=zlib.decompress(b[off:off+cs]); break
if dec is None: die('mes_system_e missing')
for off in (0xDE24,0xDE44):
 if struct.unpack_from('<I',dec,off)[0] != 0x007E0ACE: die(f'left-right typo not fixed at {off:#x}')
print('OK 좌우 속도 glyph token x2')

# Manual pages must be physically present in RomFS, 1..5, exact framebuffer size.
man=ROOT/'romfs/manual'; manifest=json.loads((man/'manifest.json').read_text(encoding='utf-8'))
if len(manifest)!=5: die('manual manifest is not 5 pages')
for i in range(1,6):
 p=man/f'page{i}.rgb565'
 if not p.is_file() or p.stat().st_size!=1280*720*2: die(f'page{i}.rgb565 missing/wrong size')
 if not (ROOT/'manual_sources'/f'page{i}_source.png').is_file(): die(f'page{i} source png missing')
 print('OK manual page',i,sha(p))

ips=ROOT/'romfs/payload/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips'
expected_ips=b'IPS32'+bytes.fromhex('00 CE 65 68 00 04 31 00 00 14')+b'EEOF'
if ips.read_bytes()!=expected_ips: die('CrashFix IPS mismatch')
print('OK CrashFix IPS',sha(ips))

main=(ROOT/'source/main.c').read_text(encoding='utf-8')
for needle in ['romfs:/manual/page%d.rgb565','HidNpadButton_L','HidNpadButton_R','HidNpadButton_A','MANUAL CrashFix IPS','Msg2Resource_e.arc']:
 if needle not in main: die('main.c missing '+needle)
print('OK gallery is connected to app startup and installer payload')

# Source RomFS payload size is a hard guard against the previous no-photo build.
romfs_bytes=sum(p.stat().st_size for p in (ROOT/'romfs').rglob('*') if p.is_file())
if romfs_bytes < 25_000_000: die(f'RomFS too small; photos likely missing: {romfs_bytes}')
print('OK RomFS physical payload bytes',romfs_bytes)
print('PREFLIGHT PASS')
