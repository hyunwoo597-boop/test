# Extract the exact installer Title ID from the known-good base NSP using the user's own prod.keys.
# This avoids inventing/changing the already-installed application's Title ID.
from pathlib import Path
import struct,sys,re
from cryptography.hazmat.primitives.ciphers import Cipher,algorithms,modes
nsp=Path(sys.argv[1]).read_bytes(); keys=Path(sys.argv[2]).read_text(errors='ignore')
m=re.search(r'^header_key\s*=\s*([0-9a-fA-F]{64})\s*$',keys,re.M)
if not m: raise SystemExit('header_key missing from prod.keys')
key=bytes.fromhex(m.group(1))
if nsp[:4]!=b'PFS0': raise SystemExit('base is not NSP')
n,ss=struct.unpack_from('<II',nsp,4); sb=16+n*24; data=(sb+ss+15)&~15
candidates=[]
for i in range(n):
 off,size,no,_=struct.unpack_from('<QQII',nsp,16+i*24); end=nsp.find(b'\0',sb+no); name=nsp[sb+no:end].decode()
 if name.endswith('.nca') and not name.endswith('.cnmt.nca'): candidates.append((size,data+off,name))
# Program NCA is the largest non-CNMT NCA.
size,pos,name=max(candidates)
sec=nsp[pos+0x200:pos+0x400]
for endian in ('little','big'):
 tweak=(1).to_bytes(16,endian)
 for k in (key,key[16:]+key[:16]):
  try:
   dec=Cipher(algorithms.AES(k),modes.XTS(tweak)).decryptor().update(sec)
  except Exception: continue
  if dec[:4] in (b'NCA2',b'NCA3'):
   tid=struct.unpack_from('<Q',dec,0x10)[0]
   print(f'{tid:016X}')
   raise SystemExit(0)
raise SystemExit('Could not decrypt base NCA header / derive Title ID')
