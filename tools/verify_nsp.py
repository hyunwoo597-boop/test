from pathlib import Path
import struct,sys
p=Path(sys.argv[1]); b=p.read_bytes()
if b[:4]!=b'PFS0': raise SystemExit('FAIL: output is not PFS0 NSP')
n,ss=struct.unpack_from('<II',b,4); strbase=16+n*24
files=[]
for i in range(n):
 off,size,no,_=struct.unpack_from('<QQII',b,16+i*24)
 end=b.find(b'\0',strbase+no); name=b[strbase+no:end].decode('ascii','replace'); files.append((name,size))
print('NSP files:',files)
largest=max((s for n,s in files if n.endswith('.nca')),default=0)
# Previous photo-less program NCA was ~16.48 MB. 5 x RGB565 adds 9,216,000 bytes.
if largest < 25_000_000: raise SystemExit(f'FAIL: largest NCA only {largest} bytes; manual images were not packed')
print('PASS: generated NSP contains a >=25MB NCA:',largest)
