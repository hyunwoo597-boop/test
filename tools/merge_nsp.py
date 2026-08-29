#!/usr/bin/env python3
import argparse, os, struct, sys
from pathlib import Path

ALIGN=0x10

def align(x,a=ALIGN): return (x+a-1)&~(a-1)

def read_pfs0(path):
    p=Path(path); data=p.read_bytes()
    if data[:4]!=b'PFS0': raise ValueError(f'{p}: not PFS0/NSP')
    n, st_size, _ = struct.unpack_from('<III',data,4)
    ents=[]; ent_off=0x10; str_off=0x10+n*0x18; data_off=str_off+st_size
    for i in range(n):
        off,size,nameoff,_=struct.unpack_from('<QQII',data,ent_off+i*0x18)
        end=data.find(b'\0',str_off+nameoff,str_off+st_size)
        name=data[str_off+nameoff:end].decode('utf-8')
        ents.append((name,data[data_off+off:data_off+off+size]))
    return ents

def write_pfs0(path, entries):
    names=b''; nameoffs=[]
    for name,_ in entries:
        nameoffs.append(len(names)); names += name.encode()+b'\0'
    n=len(entries); hdr_size=0x10+n*0x18+len(names); data_off=hdr_size
    offsets=[]; cur=0
    for _,blob in entries:
        offsets.append(cur); cur += len(blob)
    out=bytearray(data_off+cur)
    out[:4]=b'PFS0'; struct.pack_into('<III',out,4,n,len(names),0)
    for i,((name,blob),noff,off) in enumerate(zip(entries,nameoffs,offsets)):
        struct.pack_into('<QQII',out,0x10+i*0x18,off,len(blob),noff,0)
    out[0x10+n*0x18:0x10+n*0x18+len(names)] = names
    for (_,blob),off in zip(entries,offsets): out[data_off+off:data_off+off+len(blob)] = blob
    Path(path).write_bytes(out)

def main():
    ap=argparse.ArgumentParser(description='Merge two NSP PFS0 containers into one multi-title NSP')
    ap.add_argument('nsp1'); ap.add_argument('nsp2'); ap.add_argument('output')
    a=ap.parse_args()
    allents=[]; seen={}
    for src in (a.nsp1,a.nsp2):
        for name,blob in read_pfs0(src):
            if name in seen:
                # exact duplicate is safe to keep once; differing collision is fatal
                if seen[name] == blob: continue
                raise SystemExit(f'Filename collision with different content: {name}')
            seen[name]=blob; allents.append((name,blob))
    write_pfs0(a.output,allents)
    print(f'Created {a.output}: {len(allents)} files, {Path(a.output).stat().st_size} bytes')

if __name__=='__main__': main()
