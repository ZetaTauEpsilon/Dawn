"""Compare recovered waypoint rules to the supplied release under CPU emulation.

The release is never loaded or injected. Only the locally compiled test bridge
is loaded; the pinned release's pure functions run in Unicorn.
"""
import ctypes as C
import random
import struct as S
from waypoint_release_oracle import Oracle, ROOT, BASE

bridge = C.CDLL(str(ROOT/'build/waypoint-recovery-20260920/tests/waypoint_test_bridge.dll'))
bridge.run_rule.argtypes=[C.c_uint,C.c_void_p,C.c_uint,C.c_uint,C.c_void_p]
bridge.run_rule.restype=C.c_uint
bridge.canonical.argtypes=[C.c_uint,C.c_uint,C.c_void_p,C.c_void_p]
oracle=Oracle()
rng=random.Random(152)
checks=0

def u32(b,o,v): S.pack_into('<I',b,o,v&0xffffffff)
def u64(b,o,v): S.pack_into('<Q',b,o,v&0xffffffffffffffff)
def marker(kind,event,frame):
    result=C.create_string_buffer(28)
    bridge.canonical(kind,event,C.create_string_buffer(bytes(frame)),result)
    return result.raw

def objective(event,m):
    return S.pack('<III',event,0,3)+m+bytes([1,1,0,0])

def component(definition,event,m,index):
    b=bytearray(0xb10)
    u32(b,0,definition);u32(b,4,0x80804f54);u64(b,8,0xb88)
    u32(b,0x48,13);u32(b,0x4c,0x80804f53);u32(b,0x478,index)
    for i in range(3): b[0x198+i*0xf8]=1
    if index<3:
        row=0x190+index*0xf8;u32(b,row,event);b[row+8]=0
        reg,_,typ,slot=S.unpack_from('<IIHH',m)
        u32(b,row+0x68,reg or 0x811c9dc5);u32(b,row+0x6c,(typ|(slot<<16)) if reg else 0xffff00ff)
        loc=S.unpack_from('<IIII',m,12)
        b[row+0x78:row+0x88]=S.pack('<IIII',*([0x811c9dc5]*4 if loc[0] in (0,0x811c9dc5) else loc))
    for i in range(13):b[0x484+i*0x80]=3
    return b

def compare(kind,rva,b,context,frame,label):
    global checks
    oracle.cursor=0x10000000
    p=oracle.alloc(b);span=oracle.alloc(S.pack('<QQ',p,len(b)));f=oracle.alloc(frame)
    args=(span,f) if kind==3 else (span,) if kind in (4,5) else (span,context,f)
    expected=oracle.call(rva,*args)&0xffff00ff
    if kind in (4,5):expected&=255
    expected_bytes=bytes(oracle.cpu.mem_read(p,len(b)))
    local=C.create_string_buffer(bytes(b));state=C.create_string_buffer(bytes(frame))
    actual=bridge.run_rule(kind,local,len(b),context,state)&0xffff00ff
    assert actual==expected,(label,'result',hex(actual),hex(expected))
    assert local.raw[:len(b)]==expected_bytes,(label,'bytes',[(hex(i),a,e) for i,(a,e) in enumerate(zip(local.raw,expected_bytes)) if a!=e][:12])
    checks+=1
    # Compare repeat publication too: should not continually re-register a stable route.
    oracle.cpu.mem_write(p,expected_bytes)
    expected=oracle.call(rva,*args)&0xffff00ff
    if kind in (4,5):expected&=255
    actual=bridge.run_rule(kind,local,len(b),context,state)&0xffff00ff
    assert actual==expected,(label,'repeat',hex(actual),hex(expected))
    checks+=1

assert [bridge.layout(i) for i in range(8)]==[0x24e8,0x2410,0xf00,0xeb0,0xcf8,0xef8,0xbb8,0x50]

deep_events=[0xb035525a,0xf8f223a7,0x149b4756,0x772f4471,0x7811b582,0x1a188e6e,0x2a751789,0xcb573be1]
for event in deep_events:
    m=marker(0,event,b'\0');obj=objective(event,m)
    oracle.cursor=0x10000000;out=oracle.alloc(bytes(28));oracle.call(0x26c1b0,out,event)
    assert bytes(oracle.cpu.mem_read(out,28))==m,('deep canonical',hex(event))
    for context in (4,19,0):
        for index in range(4):
            b=component(0x80b565df,event,m,index)
            compare(0,0x26c850,b,context,obj,('deep',hex(event),context,index))
            if index<3:
                for offset in (0,4,8,0x48,0x4c,0x50,0x190+index*0xf8,0x198+index*0xf8,0x1f8+index*0xf8,0x208+index*0xf8):
                    invalid=b.copy();invalid[offset]^=0x80
                    compare(0,0x26c850,invalid,context,obj,('deep invalid',hex(offset)))
print('Deep Storage route cases:',checks,flush=True)

for event in (0x025cc54f,0xcb573be1):
    for section in (4,5,6,7):
        for flags in range(128):
            f=bytearray(0xf00);f[0]=1;f[5]=section
            for bit,offset in enumerate((0xd00,0xd03,0xd08,0xd0b,0xd15,0xef8,0xef9)):f[offset]=(flags>>bit)&1
            m=marker(0,event,f);f[0xeb0:0xedc]=objective(event,m)
            b=component(0x80b565df,event,m,flags%3)
            compare(1,0x26c250,b,19,f,('deep final',hex(event),section,flags))
print('Deep Storage final cases:',checks,flush=True)

beyond_events=[0x29bfce5a,0x2fda4350,0xb1ad777d,0x9225aef3,0xd60fa7df,0xbe5f8e8b,0x142ec956,0x64b46f54,0x5569523a,0x5ad156f5]
for event in beyond_events:
    for section in range(8):
        for context in (4,8,15,18,19):
            for bits in (0,127,rng.randrange(128)):
                f=bytearray(0x24e8);f[7]=1;f[13]=section
                f[0:7]=bytes((bits>>i)&1 for i in range(7));f[10]=rng.randrange(2);f[12]=rng.randrange(2);f[41]=rng.randrange(2)
                for i in range(0x16d):f[0xf4+i*8]=f[0xf7+i*8]=1
                m=marker(1,event,f);f[0x2410:0x243c]=objective(event,m)
                oracle.cursor=0x10000000;out=oracle.alloc(bytes(28));p=oracle.alloc(f);oracle.call(0x26ce70,out,event,p)
                assert bytes(oracle.cpu.mem_read(out,28))==m,('beyond canonical',hex(event))
                b=component(0x80f46225,event,m,bits%3)
                compare(2,0x26d010,b,context,f,('beyond',hex(event),section,context,bits))
print('Beyond Infinity cases:',checks,flush=True)

for kind,definition,reg,slot,event,target,targetslot in [(4,0x80f474c0,0x2763ec90,14,0xf150883c,0x45b69c3c,2),(4,0x80f54ac8,0x2763ec90,14,0xf150883c,0x45b69c3c,2),(5,0x80f47501,0x2763ec91,28,0x489890f3,0xa9350228,5)]:
    for index in range(3):
        m=S.pack('<IIHHIIII',target,1,47,targetslot,0,0,0,0);b=component(definition,event,m,index)
        u32(b,0x180,reg);u32(b,0x184,70|(slot<<16))
        for i in range(12):b[0x484+i*0x80]=0
        compare(kind,0x26ba50 if kind==4 else 0x26be70,b,0,b'\0',('strike',kind,index))
        for offset in (0,4,8,0x48,0x4c,0x50,0x180,0x184,0x186,0x190+index*0xf8,0x198+index*0xf8,0xa84,0xa98,*range(0x484,0xa80,0x80)):
            invalid=b.copy();invalid[offset]^=1
            compare(kind,0x26ba50 if kind==4 else 0x26be70,invalid,0,b'\0',('strike invalid',kind,hex(offset)))
trial_events=[0xec217779,0xda95ae49,0x183f9715,0x6fb8a85a,0xe58bb2f6,0x882dd31e,0x708b9351,0]
for event in trial_events:
    m=marker(2,event,b'\0')
    oracle.cursor=0x10000000;out=oracle.alloc(bytes(44));oracle.call(0x2709a0,out,event)
    assert bytes(oracle.cpu.mem_read(out,28))==m,('trial canonical',hex(event))
    for index in range(4):
        f=bytearray(0x90);f[0]=1;f[0x50:0x7c]=objective(event,m)
        b=component(0x80b2e706,event,m,index)
        compare(3,0x270fb0,b,0,f,('trial',hex(event),index))
        if index<3:
            for offset in (0,4,8,0x48,0x4c,0x50,0x190+index*0xf8,0x198+index*0xf8,0x1f8+index*0xf8,0x208+index*0xf8):
                invalid=b.copy();invalid[offset]^=0x80
                compare(3,0x270fb0,invalid,0,f,('trial invalid',hex(event),hex(offset)))
print('A Deadly Trial cases:',checks,flush=True)

for event in (0x6fb50106,0x90bb4c60,0xce80dbb4,0xa997b644,0xebc085fc,0x6418963e,0xb7c0b10f,0x396ed216,0x071ff5f8,0,0xffffffff):
    oracle.cursor=0x10000000;out=oracle.alloc(bytes(28));oracle.call(0x5cceb0,out,event)
    assert bytes(oracle.cpu.mem_read(out,28))==marker(4,event,b'\0'),('New Light canonical',hex(event))
    checks+=1
print('New Light cases:',checks,flush=True)

hijacked_events=[0xac66aad0,0xebbb3bec,0xae1fb52d,0x9f0648d1,0xbb71b72b,0xd014e2da,0xbcfccb52,0xededcbb4]
areas=[0x37a08717,0x849e9c59,0x849e9c59,0x849e9c59,0x849e9c59,0x37a08717,0x29c88401,0x29c88401]
for i,event in enumerate(hijacked_events):
    # The release stores the eight source identities at 7A4CB0; 5B3F20
    # supplies the area-qualified locator for its type-47 sources.
    a=bytes(oracle.cpu.mem_read(BASE+0x7a4cb0+i*12,12));reg,_,typ,slot=S.unpack('<IIHH',a)
    expected=a+(S.pack('<IIII',0xf995e43a,areas[i],reg,0x1811ef12) if typ==47 else bytes(16))
    assert marker(3,event,b'\0')==expected,('Hijacked canonical',i)
    checks+=1
    for index in range(4):
        for count in (0,1,2,16,17):
            for mutation in range(11):
                f=bytearray(0xc00);f[0]=1;f[0xbb8:0xbe4]=objective(event,expected)
                b=component(0x80b4241c,event,expected,index);banner=bytearray(0x1488);u64(banner,0x1480,count)
                # With two identical matches the observer must not acknowledge.
                for j in range(min(count,16)):
                    row=j*0x148;banner[row]=2;u32(banner,row+4,event*0x502c3f11)
                    u32(banner,row+8,0xf995e43a);u32(banner,row+0x10,0x77852db9);u64(banner,row+0x110,1)
                if mutation<6:banner[(0,4,8,0x10,0x70,0x110)[mutation]]^=0x80
                elif mutation==6:f[2]=1
                elif mutation==7:f[0xbe1]=0
                elif mutation==8:u32(b,0,0x80b4241d)
                elif mutation==9 and index<3:b[0x194+index*0xf8]=1
                oracle.cursor=0x10000000;p=oracle.alloc(b);span=oracle.alloc(S.pack('<QQ',p,len(b)))
                bp=oracle.alloc(banner);bs=oracle.alloc(S.pack('<QQ',bp,len(banner)));fp=oracle.alloc(f)
                result=oracle.call(0x272ae0,span,bs,fp)&0xffffffff
                local=C.create_string_buffer(bytes(b+f));state=C.create_string_buffer(bytes(banner))
                actual=bridge.run_rule(6,local,len(b),0,state)
                assert actual==result,('Hijacked banner',i,index,count,mutation,hex(actual),hex(result))
                assert local.raw[:len(b+f)]==b+f
                checks+=1
print('Hijacked cases:',checks,flush=True)

bridge.wire.argtypes=[C.c_void_p,C.c_uint,C.c_void_p,C.c_void_p,C.c_uint,C.c_int,C.c_int]
bridge.wire.restype=C.c_uint
for typ in (0,1,4,47,60):
    for revision in (0,1,2,3,4,8):
        for authored in (0,1):
            for loc in ((0,0,0,0),(0x811c9dc5,0,0,0),(0x4a26ad57,0x441515c2,0x777fbae8,0x12345678)):
                m=S.pack('<IIHHIIII',0x12345678 if typ else 0,0x80b12345 if typ else 0,typ,448,*loc)
                o=bytearray(objective(0xec217779,m));u32(o,8,revision)
                if rng.randrange(5)==0:o[40]=0
                if rng.randrange(7)==0:o[41]=0
                audience=S.pack('<IIHH',0x12345678 if rng.randrange(2) else 0,0x80b12345,70,4)
                current,target=rng.choice([(-1,-1),(0,3),(2,3),(4,3)])
                oracle.cursor=0x10000000;p=oracle.alloc(bytes(1024));w=oracle.alloc(S.pack('<QQQBB6x',p,1024,0,0,0))
                op=oracle.alloc(o);ap=oracle.alloc(audience)
                ok=oracle.call(0x2bd920,w,op,ap,authored,1,current,target)&255
                written=S.unpack('<Q',oracle.cpu.mem_read(w+16,8))[0] if ok else 0
                local=C.create_string_buffer(1024)
                actual=bridge.wire(local,1024,C.create_string_buffer(bytes(o)),C.create_string_buffer(audience),authored,current,target)
                assert actual==written,('wire bits',typ,revision,authored,actual,written)
                assert local.raw==bytes(oracle.cpu.mem_read(p,1024)),('wire bytes',typ,revision,authored,loc,current,target)
                checks+=1
for index in range(4):
    # Gateway's authored MarkerCapability table contains all four exact 28-byte
    # identities, including the corrected type-60/448 entrance and Vance return.
    m=marker(5,index,b'\0')
    assert oracle.pe.__data__.find(m)>=0,('Gateway marker absent from release',index,m.hex())
    checks+=1
print('Gateway cases:',checks,flush=True)

bridge.merge_meshes.argtypes=[C.c_void_p];bridge.merge_meshes.restype=C.c_uint
for count in range(6):
    for _ in range(100):
        values=[rng.choice([0,0xffffffff,0x80be0e80,0x80b2ed21,0x81234567,rng.randrange(1<<32)]) for i in range(4)]
        original=S.pack('<IIIII',count,*values)
        oracle.cursor=0x10000000;p=oracle.alloc(original);out=oracle.alloc(bytes(24));oracle.call(0x271690,out,p)
        released=bytes(oracle.cpu.mem_read(out,22));local=C.create_string_buffer(original)
        ok=bridge.merge_meshes(local)
        assert bool(ok)==bool(released[20]),('mesh merge acceptance',count,values)
        assert local.raw[:20]==(released[:20] if ok else original),('mesh merge output',count,values)
        checks+=1
bridge.valid_mesh.argtypes=[C.c_uint,C.c_void_p,C.c_uint,C.c_void_p,C.c_uint];bridge.valid_mesh.restype=C.c_uint
for index,mesh in enumerate([(0x80be0e85,0x80be0e80,111120,1060,2335),(0x80b2ed22,0x80b2ed21,1584,16,34),(0x80c4c37d,0x80c4c37c,68240,708,1574)]):
    for mutation in range(20):
        d=bytearray(0xe0);r=bytearray(0x80);u64(d,0,0xe0);u32(d,0xc4,0x80809b48);u32(d,0xd8,mesh[1])
        u64(r,0,mesh[2]);u64(r,0x28,mesh[3]);u64(r,0x38,mesh[4]);u64(r,0x30,0x50);u64(r,0x40,0x50)
        if mutation<3:d[(0,0xc4,0xd8)[mutation]]^=1
        elif mutation<6:r[(0,0x28,0x38)[mutation-3]]^=1
        elif mutation<10:u64(r,0x30 if mutation<8 else 0x40,mesh[2] if mutation%2 else 0xffffffffffffffff)
        elif mutation<12:d=d[:(0xdf if mutation==10 else 0)]
        elif mutation<14:r=r[:(0x7f if mutation==12 else 0)]
        oracle.cursor=0x10000000;mp=oracle.alloc(S.pack('<IIQQQ',*mesh));dp=oracle.alloc(d or b'\0');rp=oracle.alloc(r or b'\0')
        ds=oracle.alloc(S.pack('<QQ',dp,len(d)));rs=oracle.alloc(S.pack('<QQ',rp,len(r)))
        expected=oracle.call(0x2714d0,mp,ds,rs)&255
        actual=bridge.valid_mesh(index,C.create_string_buffer(bytes(d)),len(d),C.create_string_buffer(bytes(r)),len(r))
        assert actual==expected,('mesh validation',index,mutation)
        checks+=1
print('PASS release differential checks:',checks,flush=True)
lifecycle=bridge.lifecycle_checks()
assert 0<lifecycle<0x10000,('local lifecycle assertion line',lifecycle>>16)
print('PASS owner/readiness/banner lifecycle checks:',lifecycle,flush=True)
