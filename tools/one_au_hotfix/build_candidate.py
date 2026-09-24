"""Reproduce the hotfix on the exact entrance-fixed, no-wipe 1AU candidate.

Requires pefile. The assembler object is built from activity-script.asm with ML64.
Never substitutes the upstream non-1AU DLL. Does not install or modify the input.
"""
import argparse, hashlib, json, struct, shutil, zipfile
from datetime import datetime, timezone
from pathlib import Path
import pefile

HERE=Path(__file__).resolve().parent
INPUT_SHA='ea5de3644dc5e7436393f8aa81297ad14f7cb970939975de37891ed8feb48c71'
HOTFIX_SHA='e49a249fc1d2ec161978063fc79d8a3aa9d61d1bf56fb62fb5d29242fddcccb4'
RELEASE='0.1.3-1au-hotfix-no-wipe'
def sha(data):return hashlib.sha256(data).hexdigest()
def align(n,a):return (n+a-1)//a*a
def relative(op,at,target):return bytes([op])+struct.pack('<i',target-at-5)

def coff_object(raw,code_rva,unwind_rva):
    machine,count,_,symptr,symcount,optional,_=struct.unpack_from('<HHIIIHH',raw)
    assert machine==0x8664 and optional==0
    sections=[]
    for i in range(count):
        name,_,_,size,ptr,relptr,_,rels,_,_=struct.unpack_from('<8sIIIIIIHHI',raw,20+i*40)
        sections.append(dict(name=name.rstrip(b'\0').decode(),data=bytearray(raw[ptr:ptr+size]),relptr=relptr,rels=rels))
    strings=raw[symptr+symcount*18:]
    symbols={}
    i=0
    while i<symcount:
        name,value,section,_,_,aux=struct.unpack_from('<8sIhHBB',raw,symptr+i*18)
        if name[:4]==b'\0'*4:
            offset=struct.unpack_from('<I',name,4)[0];name=strings[offset:].split(b'\0',1)[0]
        else:name=name.rstrip(b'\0')
        symbols[i]=(name.decode(),value,section);i+=1+aux
    bases={i+1:(code_rva if s['name']=='.text$mn' else unwind_rva) for i,s in enumerate(sections) if s['name'] in ('.text$mn','.xdata')}
    for section in sections:
        if section['name'] not in ('.text$mn','.pdata'):continue
        for j in range(section['rels']):
            offset,symbol,kind=struct.unpack_from('<IIH',raw,section['relptr']+j*10)
            name,value,target_section=symbols[symbol]
            target=bases[target_section]+value if target_section else {'writer_write':0x46b260}[name]
            if kind==4:
                assert section['name']=='.text$mn'
                addend=struct.unpack_from('<i',section['data'],offset)[0]
                struct.pack_into('<i',section['data'],offset,target+addend-(code_rva+offset+4))
            else:
                assert kind==3 and section['name']=='.pdata'
                addend=struct.unpack_from('<I',section['data'],offset)[0]
                struct.pack_into('<I',section['data'],offset,target+addend)
    return {s['name']:bytes(s['data']) for s in sections}

def patch_dll(original,obj):
    assert len(original)==26295296 and sha(original)==INPUT_SHA,'Unsupported input DLL'
    p=pefile.PE(data=original)
    assert p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size==0,'Signed input unsupported'
    assert len(original)==max(s.PointerToRawData+s.SizeOfRawData for s in p.sections),'Overlay unsupported'
    section_rva=align(max(s.VirtualAddress+s.Misc_VirtualSize for s in p.sections),p.OPTIONAL_HEADER.SectionAlignment)
    # .text$mn has exactly 320 bytes, including read-only field constants.
    chunks=coff_object(obj,section_rva,section_rva+0x140)
    assert len(chunks['.text$mn'])==0x140 and len(chunks['.pdata'])==12
    section=bytearray(chunks['.text$mn']+chunks['.xdata'])
    section.extend(b'\0'*(align(len(section),4)-len(section)))
    exception_offset=len(section)
    old_exception=p.OPTIONAL_HEADER.DATA_DIRECTORY[3]
    entries=p.get_data(old_exception.VirtualAddress,old_exception.Size)
    new_entry=chunks['.pdata']
    assert all(struct.unpack_from('<I',entries,i)[0]<section_rva for i in range(0,len(entries),12))
    section.extend(entries+new_entry)
    patched=bytearray(original);ranges=[]
    def replace(start,end,code,label):
        assert len(code)<=end-start
        off=p.get_offset_from_rva(start)
        before=original[off:off+end-start]
        after=code+b'\x90'*(end-start-len(code))
        patched[off:off+end-start]=after
        ranges.append(dict(name=label,rva=start,offset=off,before=before.hex(),after=after.hex()))
    start=0x419240;code=bytearray.fromhex('4c897c24204d8bc7418b542404418b0c24')
    code.extend(relative(0xe8,start+len(code),0x3892e0))
    code.extend(b'\x84\xc0\x0f\x84'+struct.pack('<i',0x419420-(start+len(code)+8)))
    code.extend(bytes.fromhex('488d4c2420'))
    code.extend(relative(0xe8,start+len(code),0x419460))
    code.extend(b'\x84\xc0\x0f\x84'+struct.pack('<i',0x419420-(start+len(code)+8)))
    code.extend(relative(0xe9,start+len(code),0x4192b3))
    replace(start,0x4192b3,code,'Omega full roster: exact identity lookup then existing descriptor checks')
    for start,end,failure,args,closure in [
        (0x420608,0x420670,0x4207b2,'4d8bc68b54246c8bcf','488d4c2460'),
        (0x420c0c,0x420c73,0x420de3,'4d8bc78b54244c8b4c2448','488d4c2440')]:
        code=bytearray.fromhex(args)
        code.extend(relative(0xe8,start+len(code),0x3892e0))
        code.extend(b'\x84\xc0\x0f\x84'+struct.pack('<i',failure-(start+len(code)+8)))
        code.extend(bytes.fromhex(closure))
        code.extend(relative(0xe8,start+len(code),0x420800))
        code.extend(b'\x84\xc0\x0f\x84'+struct.pack('<i',failure-(start+len(code)+8)))
        code.extend(relative(0xe9,start+len(code),end))
        replace(start,end,code,'Omega reveal/boss/crown: exact identity lookup then existing descriptor checks')
    for start,end,done in [(0x451ceb,0x451d70,0x451ef0),(0x27616f,0x276202,0x2763ba)]:
        code=bytearray.fromhex('488bd3498bce') # snapshot rbx, writer r14
        code.extend(relative(0xe8,start+len(code),section_rva))
        code.extend(relative(0xe9,start+len(code),done))
        replace(start,end,code,'Activity script HUD: preserve state and suppress Omega ring')
    # Add one RX section, including ML64-generated unwind data and the complete
    # original sorted exception directory plus this helper's entry.
    header=p.sections[-1].get_file_offset()+40
    assert header+40<=p.OPTIONAL_HEADER.SizeOfHeaders
    raw_offset=align(len(patched),p.OPTIONAL_HEADER.FileAlignment)
    raw_size=align(len(section),p.OPTIONAL_HEADER.FileAlignment)
    struct.pack_into('<8sIIIIIIHHI',patched,header,b'.auhotfx',len(section),section_rva,raw_size,raw_offset,0,0,0,0,0x60000020)
    struct.pack_into('<H',patched,p.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),p.FILE_HEADER.NumberOfSections+1)
    struct.pack_into('<I',patched,p.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfImage'),align(section_rva+len(section),p.OPTIONAL_HEADER.SectionAlignment))
    struct.pack_into('<I',patched,p.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfCode'),p.OPTIONAL_HEADER.SizeOfCode+raw_size)
    struct.pack_into('<II',patched,old_exception.get_file_offset(),section_rva+exception_offset,len(entries)+12)
    patched.extend(b'\0'*(raw_offset-len(patched)));patched.extend(section);patched.extend(b'\0'*(raw_size-len(section)))
    checksum=pefile.PE(data=bytes(patched)).generate_checksum()
    struct.pack_into('<I',patched,p.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),checksum)
    info=dict(schema=1,candidate='0.1.3-1au-no-wipe',release=RELEASE,inputSha256=INPUT_SHA,hotfixDllSha256=HOTFIX_SHA,
        outputSha256=sha(patched),originalSize=len(original),outputSize=len(patched),helperRva=section_rva,
        helperSize=struct.unpack('<III',new_entry)[1]-section_rva,unwindRva=section_rva+0x140,
        objectSha256=sha(obj),ranges=ranges)
    return bytes(patched),info

def validate_payload(root,manifest):
    seen=set()
    for entry in manifest['files']:
        rel=Path(entry['path']);assert not rel.is_absolute() and '..' not in rel.parts
        assert entry['path'] not in seen;seen.add(entry['path'])
        data=(root/'payload'/rel).read_bytes();assert len(data)==entry['size'] and sha(data)==entry['sha256']
    assert 'Dawn/scripts/one_au.lua' in seen

def main():
    parser=argparse.ArgumentParser();parser.add_argument('candidate',type=Path);parser.add_argument('hotfix',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();source=args.candidate.resolve();out=args.output.resolve();archive=Path(str(out)+'.zip')
    assert out!=source and source not in out.parents and not out.exists() and not archive.exists()
    manifest=json.loads((source/'release.json').read_text(encoding='utf-8-sig'))
    assert manifest['release']=='0.1.3-1au-no-wipe' and manifest['gameBuild']==86657
    validate_payload(source,manifest)
    with zipfile.ZipFile(args.hotfix) as hotfix:
        hotfix_manifest=json.loads(hotfix.read('release.json'))
        assert hotfix_manifest['release']=='0.1.3-omega-fix'
        assert sha(hotfix.read('payload/steam_api64.dll'))==HOTFIX_SHA
        for entry in hotfix_manifest['files']:
            data=hotfix.read('payload/'+entry['path']);assert sha(data)==entry['sha256'] and len(data)==entry['size']
            if entry['path']!='steam_api64.dll':assert data==(source/'payload'/entry['path']).read_bytes()
        # Core installer/updater are identical except line endings.
        for name in ('Install-Dawn.ps1','Update-Dawn.ps1','Update-Dawn.cmd'):
            assert hotfix.read(name).decode('utf-8-sig').replace('\r','')==(source/name).read_text(encoding='utf-8-sig').replace('\r','')
        dll,info=patch_dll((source/'payload/steam_api64.dll').read_bytes(),(HERE/'activity-script.obj').read_bytes())
        expected=json.loads((HERE/'candidate-patch.json').read_text())
        assert info==expected,'Recipe differs from reviewed patch'
        shutil.copytree(source,out)
        (out/'payload/steam_api64.dll').write_bytes(dll)
        for name in ('Install-Dawn.cmd','Setup-Dawn.ps1','READ-ME.txt'):
            (out/name).write_bytes(hotfix.read(name))
        provenance=out/'repair-source/hotfix';shutil.copytree(HERE,provenance,ignore=shutil.ignore_patterns('__pycache__'))
        (provenance/'upstream-release-notes.md').write_bytes(hotfix.read('RELEASE-NOTES.md'))
        (out/'RELEASE-NOTES.md').write_text((HERE/'RELEASE-NOTES.md').read_text(),encoding='utf-8')
    manifest['release']=RELEASE;manifest['createdUtc']=datetime.now(timezone.utc).isoformat()
    for e in manifest['files']:
        if e['path']=='steam_api64.dll':e['size']=len(dll);e['sha256']=sha(dll)
    (out/'release.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    validate_payload(out,manifest)
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for file in sorted(out.rglob('*')):
            if file.is_file():z.write(file,Path(out.name)/file.relative_to(out))
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for e in manifest['files']:assert sha(z.read(f'{out.name}/payload/{e["path"]}'))==e['sha256']
    Path(str(archive)+'.sha256').write_text(f'{sha(archive.read_bytes())}  {archive.name}\n')
    print(json.dumps(dict(zip=str(archive),dllSha256=sha(dll),files=len(manifest['files'])),indent=2))

if __name__=='__main__':main()
