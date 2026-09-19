"""Execute changed code offline; no DLL is loaded into Windows or the game."""
import argparse,json,struct,zipfile,itertools
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import *
from build_candidate import patch_dll,sha,HERE

STACK=0x60000000;SNAP=0x50000000;WRITER=0x50040000;BUFFER=0x50041000;STOP=0x50043000
class Machine:
    def __init__(self,data):
        self.mu=Uc(UC_ARCH_X86,UC_MODE_64);self.p=pefile.PE(data=data)
        for s in self.p.sections:
            if s.Characteristics & 0x20000000:
                self.mu.mem_map(s.VirtualAddress,(max(s.Misc_VirtualSize,s.SizeOfRawData)+4095)&~4095)
                self.mu.mem_write(s.VirtualAddress,s.get_data())
        self.mu.mem_map(STACK,0x20000);self.mu.mem_map(SNAP,0x50000)
        self.stops=set();self.stopped=None;self.callbacks={}
        self.mu.hook_add(UC_HOOK_CODE,self.hook)
    def hook(self,mu,addr,size,user):
        if addr in self.stops:self.stopped=addr;mu.emu_stop()
        elif addr in self.callbacks:self.callbacks[addr](mu)
    def run(self,start,stops):
        self.stops=set(stops);self.stopped=None
        self.mu.emu_start(start,0,count=100000)
        assert self.stopped in self.stops,'Execution did not reach the expected continuation'
    def script(self,start,done,omega,flag,state,capacity,position=0,upstream=False):
        mu=self.mu;mu.mem_write(SNAP,bytes(0x30000));mu.mem_write(BUFFER,bytes(128));mu.mem_write(STACK,bytes(0x20000))
        mu.mem_write(SNAP+0xf510,bytes([omega]))
        flag_offset,state_offset=(0x1ec78,0x1ec7c) if upstream else (0x274d0,0x274d4)
        mu.mem_write(SNAP+flag_offset,bytes([flag]));mu.mem_write(SNAP+state_offset,struct.pack('<i',state))
        mu.mem_write(WRITER,struct.pack('<QQQBB',BUFFER,capacity,position,0,0))
        rsp=STACK+0x10000-(8 if upstream else 0)
        mu.reg_write(UC_X86_REG_RSP,rsp);mu.mem_write(rsp,struct.pack('<Q',STOP))
        mu.reg_write(UC_X86_REG_RBX,SNAP);mu.reg_write(UC_X86_REG_R14,WRITER);mu.reg_write(UC_X86_REG_R13,0)
        mu.reg_write(UC_X86_REG_RCX,WRITER);mu.reg_write(UC_X86_REG_RDX,SNAP)
        self.run(start,[done])
        assert mu.reg_read(UC_X86_REG_RSP)==rsp+(8 if upstream else 0)
        return (mu.reg_read(UC_X86_REG_RAX)&255,bytes(mu.mem_read(WRITER+16,10)),bytes(mu.mem_read(BUFFER,64)))

def hud_tests(old,new,upstream):
    original,patched,golden=(Machine(data) for data in (old,new,upstream));count=0
    for omega,flag,state,capacity,position in itertools.product((False,True),(False,True),(-1,0,1,100),(0,8,44,45,48,49,64),(0,3)):
        ref=golden.script(0x464270,STOP,omega,flag,state,capacity,position,True)
        for start,done in ((0x451ceb,0x451ef0),(0x27616f,0x2763ba)):
            result=patched.script(start,done,omega,flag,state,capacity,position)
            assert result==ref,(omega,flag,state,capacity,position,hex(start),'upstream mismatch')
            if not omega:
                assert result==original.script(start,done,omega,flag,state,capacity,position),'Other mission behavior changed'
            count+=1
    return count

def roster_tests(new):
    m=Machine(new);mu=m.mu;count=0
    # Only lookup and descriptor predicates are modeled here. Source regressions
    # separately exercise both against the real published catalog and descriptors.
    for kind,found,valid in itertools.product(range(3),(False,True),(False,True)):
        mu.mem_write(STACK,bytes(0x20000));mu.reg_write(UC_X86_REG_RSP,STACK+0x10000)
        rsp=STACK+0x10000;key=0xf4d0e0b2;tag=0x80f47979;group=SNAP;expected=SNAP+0x8000
        mu.mem_write(expected,struct.pack('<II',key,tag));mu.reg_write(UC_X86_REG_R12,expected)
        mu.reg_write(UC_X86_REG_R15,group);mu.reg_write(UC_X86_REG_R14,group);mu.reg_write(UC_X86_REG_RDI,key)
        for off in (0x6c,0x4c):mu.mem_write(rsp+off,struct.pack('<I',tag))
        mu.mem_write(rsp+0x48,struct.pack('<I',key))
        mu.mem_write(rsp+0x28,struct.pack('<Q',expected))
        for off in (0x40,0x60):mu.mem_write(rsp+off,struct.pack('<Q',group))
        start,end,fail,closure=[(0x419240,0x4192b3,0x419420,0x20),(0x420608,0x420670,0x4207b2,0x60),(0x420c0c,0x420c73,0x420de3,0x40)][kind]
        calls=[]
        def ret(value):
            sp=mu.reg_read(UC_X86_REG_RSP);target=struct.unpack('<Q',mu.mem_read(sp,8))[0]
            for reg in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9,UC_X86_REG_R10,UC_X86_REG_R11):mu.reg_write(reg,0xcccccccc)
            mu.reg_write(UC_X86_REG_RAX,int(value));mu.reg_write(UC_X86_REG_RSP,sp+8);mu.reg_write(UC_X86_REG_RIP,target)
        def lookup(mu):
            assert (mu.reg_read(UC_X86_REG_RCX),mu.reg_read(UC_X86_REG_RDX),mu.reg_read(UC_X86_REG_R8))==(key,tag,group)
            calls.append('lookup');ret(found)
        def descriptors(mu):
            assert mu.reg_read(UC_X86_REG_RCX)==rsp+closure
            assert struct.unpack('<Q',mu.mem_read(rsp+closure,8))[0]==group
            calls.append('descriptors');ret(valid)
        m.callbacks={0x3892e0:lookup,0x419460:descriptors,0x420800:descriptors}
        m.run(start,[end,fail]);assert m.stopped==(end if found and valid else fail)
        assert calls==(['lookup','descriptors'] if found else ['lookup'])
        assert mu.reg_read(UC_X86_REG_RSP)==rsp
        count+=1
    return count

def preservation(old,new,recipe):
    p,q=pefile.PE(data=old),pefile.PE(data=new)
    allowed=set(range(p.OPTIONAL_HEADER.SizeOfHeaders))
    for r in recipe['ranges']:allowed.update(range(r['offset'],r['offset']+len(bytes.fromhex(r['after']))))
    assert all(a==b or i in allowed for i,(a,b) in enumerate(zip(old,new)))
    assert len(q.sections)==len(p.sections)+1 and q.verify_checksum()
    assert q.sections[-1].Characteristics==0x60000020 # no writable/executable section
    orig=[(e.struct.BeginAddress,e.struct.EndAddress,e.struct.UnwindData) for e in p.DIRECTORY_ENTRY_EXCEPTION]
    combined=[(e.struct.BeginAddress,e.struct.EndAddress,e.struct.UnwindData) for e in q.DIRECTORY_ENTRY_EXCEPTION]
    assert combined[:-1]==orig and combined[-1]==(recipe['helperRva'],recipe['helperRva']+recipe['helperSize'],recipe['unwindRva'])
    assert q.get_data(recipe['unwindRva'],12)==bytes.fromhex('010704000732037002600130')
    for number in (0,1,2,5,6,9,10,12,13):
        a,b=p.OPTIONAL_HEADER.DATA_DIRECTORY[number],q.OPTIONAL_HEADER.DATA_DIRECTORY[number]
        assert (a.VirtualAddress,a.Size)==(b.VirtualAddress,b.Size)
    # Every earlier entrance/no-wipe change lives outside the five new ranges.
    return True

def main():
    ap=argparse.ArgumentParser();ap.add_argument('candidate',type=Path);ap.add_argument('hotfix',type=Path);args=ap.parse_args()
    old=args.candidate.read_bytes();new,recipe=patch_dll(old,(HERE/'activity-script.obj').read_bytes())
    assert recipe==json.loads((HERE/'candidate-patch.json').read_text())
    with zipfile.ZipFile(args.hotfix) as z:upstream=z.read('payload/steam_api64.dll')
    preservation(old,new,recipe)
    result=dict(hudMachineCases=hud_tests(old,new,upstream),rosterMachineCases=roster_tests(new),
        earlierRepairsPreserved=True,exceptionMetadataVerified=True,dllSha256=sha(new),gameplayVerified=False)
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
