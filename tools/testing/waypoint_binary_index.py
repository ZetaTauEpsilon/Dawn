"""Offline, heuristic function matching. Matches are leads, not equivalence proof."""
import collections
import ctypes as c
from ctypes import wintypes as w
import difflib
import json
from pathlib import Path
import re
import capstone
import pefile
from native_activity_stack_budget_tests import Symbol

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/waypoint-recovery-20260920'
LOCAL = ROOT / 'build/x64/Release/steam_api64.dll'


def symbols():
    kernel = c.WinDLL('kernel32'); kernel.GetCurrentProcess.restype = w.HANDLE
    process = kernel.GetCurrentProcess()
    dbg = c.WinDLL('dbghelp', use_last_error=True)
    dbg.SymSetOptions(0x2 | 0x4)
    dbg.SymInitialize.argtypes = [w.HANDLE, c.c_char_p, w.BOOL]
    dbg.SymLoadModuleEx.argtypes = [w.HANDLE, w.HANDLE, c.c_char_p, c.c_char_p, c.c_ulonglong, w.DWORD, c.c_void_p, w.DWORD]
    dbg.SymLoadModuleEx.restype = c.c_ulonglong
    callback_type = c.WINFUNCTYPE(w.BOOL, c.POINTER(Symbol), w.ULONG, c.c_void_p)
    dbg.SymEnumSymbols.argtypes = [w.HANDLE, c.c_ulonglong, c.c_char_p, callback_type, c.c_void_p]
    dbg.SymCleanup.argtypes = [w.HANDLE]
    assert dbg.SymInitialize(process, str(LOCAL.parent).encode(), False)
    base = dbg.SymLoadModuleEx(process, None, str(LOCAL).encode(), None, 0x180000000, 0, None, 0)
    assert base
    result = []
    def emit(s, size, context):
        v = s.contents
        if v.Tag != 5 or not v.Size:
            return True
        name = c.string_at(c.addressof(v) + Symbol.Name.offset, v.NameLen).decode(errors='replace')
        result.append({'name': name, 'rva': v.Address-base, 'size': v.Size})
        return True
    cb = callback_type(emit)
    assert dbg.SymEnumSymbols(process, base, b'*', cb, None)
    dbg.SymCleanup(process)
    return result


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
def signature(pe, start, size):
    seq = []
    constants = set()
    for address, length, mnemonic, operands in md.disasm_lite(pe.get_data(start,size),start):
        if mnemonic == 'int3': continue
        if mnemonic not in ('call', 'lea') and not mnemonic.startswith('j'):
            constants.update(int(x,16) for x in re.findall(r'0x[0-9a-f]+', operands)
                             if 0x100000 <= int(x,16) <= 0xffffffff)
        operands = re.sub(r'rip [+-] 0x[0-9a-f]+', 'rip OFFSET', operands)
        if mnemonic == 'call' or mnemonic.startswith('j'): operands = 'TARGET'
        seq.append(mnemonic+' '+operands)
    return seq, constants, collections.Counter(x.split()[0] for x in seq)


if __name__ == '__main__':
    syms = symbols()
    (OUT/'local-symbols.json').write_text(json.dumps(syms,indent=2))
    pe = pefile.PE(str(OUT/'release.dll'))
    local = pefile.PE(str(LOCAL))
    functions = []
    for e in pe.DIRECTORY_ENTRY_EXCEPTION:
        start,end=e.struct.BeginAddress,e.struct.EndAddress
        if end-start<30: continue
        seq, constants, hist = signature(pe,start,end-start)
        functions.append((start,end-start,seq,constants,hist))
    rows = []
    for sym in syms:
        name=sym['name']
        mission = re.search(r'::(launchpad|gateway|beyond_infinity|deep_storage|hijacked|deadly_trial)::',name)
        if not ((mission and re.search(r'Controller::(position|publish|update_module|marker)$|write_body<|::request$',name))
                or 'coo::native_presentation::directive_record<' in name
                or 'coo::native_presentation::objective<' in name): continue
        seq, constants, hist = signature(local,sym['rva'],sym['size'])
        possible=[]
        for start,size,other,cons,other_hist in functions:
            ratio=min(len(seq),len(other))/max(1,len(seq),len(other))
            if ratio<0.20: continue
            overlap=len(constants&cons)/max(1,len(constants))
            hsim=sum((hist&other_hist).values())/max(1,len(seq),len(other))
            possible.append((overlap*2+hsim, start,size,other))
        top=[]
        for pre,start,size,other in sorted(possible,reverse=True)[:15]:
            score=difflib.SequenceMatcher(None,seq,other,autojunk=False).ratio()
            top.append({'rva':hex(start),'size':size,'score':round(score,4),'pre':round(pre,4)})
        row={**sym,'matches':sorted(top,key=lambda x:x['score'],reverse=True)[:4]}
        rows.append(row)
        print(name, row['matches'][:2],flush=True)
    (OUT/'matches.json').write_text(json.dumps(rows,indent=2))
