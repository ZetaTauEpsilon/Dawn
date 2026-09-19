"""Verify build 86657's native camera selectors without modifying the game.

Reads a running process (--pid) or mapped-section capture (--snapshot). It does
not invoke native functions, install hooks, write process memory, or prove that
a camera has been exercised in gameplay. Uses only the Python standard library.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import struct
import sys


EXPECTED_EXE_SHA256 = "81964380664e7fcee3c620085a157fdeaf91fefacf7214907820f188bbeb4ced"

# RVAs, never absolute process addresses. These names describe recovered behavior;
# they are not exported game symbols or a supported public API.
SIGNATURES = {
    "weapon_stage_submit": (0x11D5A70, "4c 8b dc 55 53 56 41 55 41 57 49 8d ab 58 f2 ff ff 48 81 ec 80 0e 00 00"),
    "weapon_stage_empty_branch": (0x11D5AA0, "40 32 f6 8b 81 0c 06 00 00 4c 8b f9 4c 63 ea 44 0f a3 e8 44 88 4c 24 41 4c 89 44 24 58 48 89 5c 24 50 0f 83 e5 07 00 00"),
    "weapon_stage_empty_returns_false": (0x11D62AD, "40 0f b6 c6 eb dc"),
    "weapon_record_required_by_world_stage": (0x11C7847, "e8 24 e2 00 00 48 8b 4b 28 8d 57 04 89 7c 24 30 41 b1 01 48 89 7c 24 28 45 33 c0 c7 44 24 20 00 00 00 02 e8 01 e2 00 00"),
    "weapon_record_type_lookup": (0x11CA8D0, "8b 81 08 06 00 00 83 e8 02 83 f8 02 76 11 48 ff c2 48 81 c1 d0 2b 00 00 49 3b d0 7c e3 eb 03 4c 8b c9"),
    "ui_packet_producer": (0x132B890, "40 55 56 57 41 54 41 56 48 8d ac 24 a0 d1 ff ff b8 60 2f 00 00"),
    "ui_prepared_type_and_frame": (0x115FD1A, "83 bb 08 06 00 00 07 75 12 48 8b 8b 88 0a 00 00 e8 81 06 00 00 48 8b cb 41 ff d6"),
    "ui_packet_frame_owner": (0x132BCD5, "48 8b 86 88 0a 00 00"),
    "ui_packet_native_empty_output": (0x132BD0F, "4c 89 a0 18 6f 03 00 4c 89 a7 b8 00 00 00"),
    "ui_packet_consumer_accepts_null": (0x116031C, "48 8b b7 18 6f 03 00 48 85 f6 74 44"),
    "clean_view_builder": (0x11D4670, "48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 48 89 7c 24 20 41 56 48 83 ec 40 49 8b d9 41 8b f8 48 8b f2 48 8b e9"),
    "model_visibility_inheritance": (0x58C280, "8b c2 83 f9 ff 74 60 83 f8 ff 74 5b 8b d1 81 e1 ff 1f 00 00"),
    "model_visibility_setter": (0x1169110, "8b c2 45 22 c1 c1 f8 0d 81 e2 ff 1f 00 00 44 8b d0 49 81 ca 00 00 fc 0f 0f b7 c0 49 c1 ea 12 4c 23 d0"),
    "render_mask_and_bvh_layout": (0x1169140, "41 0f af 52 30 8b c2 49 03 42 08 45 0f b6 d1 41 f6 d2 44 22 50 54 0f be 50 52 45 0a d0 44 88 50 54 83 ea 01 74 0e 83 fa 02 75 3e 48 81 c1 e0 00 00 00 eb 1f f3 0f 10 48 40 0f 57 c0 0f 2e c8 7a 0b 75 09 48 81 c1 b0 00 00 00 eb 07 48 81 c1 c8 00 00 00 48 85 c9 74 11 0f b7 40 50 48 8b 49 08 48 c1 e0 06 44 88 54 08 34"),
    "hud_view_link": (0x11D47BD, "48 8b 44 24 70 48 89 88 28 05 00 00 48 8b 44 24 38 48 89 05 b3 71 c8 01"),
    "weapon_view_link": (0x11D4C1F, "48 8d 05 1a a0 c8 01 0f ba f1 0a 48 89 05 e7 9f c8 01"),
    "render_proxy_target": (0x1152240, "8b 50 04"),
    "actor_identity_parent": (0x5589F0, "8b 41 0c 89 02 8b 41 3c 83 f8 ff"),
    "following_camera_update": (0x1294500, "40 55 53 41 55 41 56 48 8d ac 24 88 fe ff ff 48 81 ec 78 02 00 00 44 0f 29 84 24 20 02 00 00"),
    "following_output_orientation": (0x1294A51, "49 8d 4e 3c 49 8d 5e 48"),
    "native_offset_placement": (0x12D5A61, "4c 8d 85 98 09 00 00 48 8d 8d 60 09 00 00"),
    "get_director": (0x1284950, "48 89 5c 24 08 57 48 83 ec 20 48 8b 1d 5f f4 c9 01"),
    "select_director": (0x1289230, "48 89 5c 24 10 57 48 83 ec 20 48 63 fa 48 8d 15 bc 6d d7 fe 48 63 d9 8b 8c ba 70 96 28 01"),
    "select_camera": (0x128F290, "44 88 4c 24 20 89 54 24 10 48 89 4c 24 08 53 55 57 41 54 41 55 48 83 ec 50 48 8b 41 10 48 8d 59 10"),
    "gameplay_first_person_request": (0x1295ABC, "ba 04 00 00 00 e9 a5 00 00 00"),
    "debug_director_constructor": (0x1281190, "48 89 5c 24 10 48 89 74 24 20 57 48 83 ec 20 41 8b f0 8b fa 48 8b d9"),
    "flying_camera_constructor": (0x1281470, "48 89 5c 24 18 48 89 74 24 20 57 48 83 ec 40"),
    "flying_camera_update": (0x1293790, "4c 8b dc 55 56 41 57 49 8d ab f8 fe ff ff 48 81 ec f0 01 00 00"),
    "flying_camera_initial_pose": (0x12814D8, "48 8b d3 48 8d 4c 24 20 e8 eb 6c 0e ff 0f 28 44 24 20 48 8d 53 28 48 8d 4f 70 0f 11 47 60 e8 d5 41 0e ff"),
}
CAMERAS = (
    # mode, native name, switch case, constructor call, constructor, vtable
    (0, "following", 0x128F2F6, 0x128F321, 0x1281590, 0x1C803D0),
    (1, "orbiting", 0x128F32B, 0x128F34B, 0x12D1610, None),
    (2, "flying", 0x128F355, 0x128F367, 0x1281470, 0x1C80860),
    (4, "first person", 0x128F394, 0x128F3B9, 0x1281420, 0x1C80488),
)


class Snapshot:
    def __init__(self, root: Path):
        metadata = json.loads((root / "mapped-image.json").read_text())
        self.base = metadata["base"]
        self.sections = [(row["rva"], (root / (row["name"].lstrip(".") + ".bin")).read_bytes())
                         for row in metadata["sections"]]

    def read(self, rva: int, size: int) -> bytes:
        for start, data in self.sections:
            if start <= rva and rva + size <= start + len(data):
                return data[rva - start:rva - start + size]
        raise ValueError(f"Uncaptured range: RVA 0x{rva:X}, size {size}")

    def close(self) -> None:
        pass


class LiveImage:
    def __init__(self, pid: int, executable: Path):
        import ctypes
        from ctypes import wintypes

        if sys.platform != "win32":
            raise OSError("Live verification requires Windows; use --snapshot elsewhere")
        self.ctypes = ctypes
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.kernel.CloseHandle.restype = wintypes.BOOL
        self.kernel.ReadProcessMemory.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                                  ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        self.kernel.ReadProcessMemory.restype = wintypes.BOOL
        self.kernel.K32EnumProcessModulesEx.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.HMODULE),
                                                        wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
                                                        wintypes.DWORD]
        self.kernel.K32EnumProcessModulesEx.restype = wintypes.BOOL
        self.kernel.K32GetModuleFileNameExW.argtypes = [wintypes.HANDLE, wintypes.HMODULE,
                                                        wintypes.LPWSTR, wintypes.DWORD]
        self.kernel.K32GetModuleFileNameExW.restype = wintypes.DWORD

        class ModuleInfo(ctypes.Structure):
            _fields_ = [("base", ctypes.c_void_p), ("size", wintypes.DWORD), ("entry", ctypes.c_void_p)]

        self.kernel.K32GetModuleInformation.argtypes = [wintypes.HANDLE, wintypes.HMODULE,
                                                        ctypes.POINTER(ModuleInfo), wintypes.DWORD]
        self.kernel.K32GetModuleInformation.restype = wintypes.BOOL
        # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ. No process-write rights.
        self.handle = self.kernel.OpenProcess(0x0400 | 0x0010, False, pid)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            modules = (wintypes.HMODULE * 1024)()
            needed = wintypes.DWORD()
            if not self.kernel.K32EnumProcessModulesEx(self.handle, modules, ctypes.sizeof(modules),
                                                       ctypes.byref(needed), 3):
                raise ctypes.WinError(ctypes.get_last_error())
            if needed.value > ctypes.sizeof(modules):
                raise ValueError("Process module list exceeds verification capacity")
            for module in modules[:needed.value // ctypes.sizeof(wintypes.HMODULE)]:
                name = ctypes.create_unicode_buffer(32768)
                count = self.kernel.K32GetModuleFileNameExW(self.handle, module, name, len(name))
                if count == 0 or count >= len(name):
                    raise ctypes.WinError(ctypes.get_last_error())
                path = Path(name.value)
                if path.name.lower() != "destiny2.exe":
                    continue
                if path.resolve() != executable.resolve():
                    raise ValueError("The running executable is not the supplied --exe")
                info = ModuleInfo()
                if not self.kernel.K32GetModuleInformation(self.handle, module, ctypes.byref(info),
                                                           ctypes.sizeof(info)):
                    raise ctypes.WinError(ctypes.get_last_error())
                self.base, self.size = info.base, info.size
                break
            else:
                raise ValueError("The process does not contain destiny2.exe")
        except Exception:
            self.close()
            raise

    def read(self, rva: int, size: int) -> bytes:
        if rva < 0 or size <= 0 or rva + size > self.size:
            raise ValueError("Read exceeds the main executable image")
        buffer = self.ctypes.create_string_buffer(size)
        copied = self.ctypes.c_size_t()
        if not self.kernel.ReadProcessMemory(self.handle, self.base + rva, buffer, size,
                                              self.ctypes.byref(copied)) or copied.value != size:
            raise OSError(f"Unable to read RVA 0x{rva:X}")
        return buffer.raw

    def close(self) -> None:
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


def verify(image, exe_hash: str) -> dict:
    checks = []

    def check(name, actual, expected):
        checks.append({"name": name, "passed": actual == expected,
                       "actual": actual, "expected": expected})

    def u32(rva):
        return struct.unpack("<I", image.read(rva, 4))[0]

    def pointer_rva(rva):
        return struct.unpack("<Q", image.read(rva, 8))[0] - image.base

    def call_target(rva):
        data = image.read(rva, 5)
        if data[0] != 0xE8:
            return None
        return rva + 5 + struct.unpack_from("<i", data, 1)[0]

    check("executable_sha256", exe_hash, EXPECTED_EXE_SHA256)
    for name, (rva, encoded) in SIGNATURES.items():
        expected = bytes.fromhex(encoded)
        check(name, image.read(rva, len(expected)).hex(), expected.hex())
    for mode, name, case, call, constructor, vtable in CAMERAS:
        prefix = f"camera_{mode}_{name.replace(' ', '_')}"
        string_rva = pointer_rva(0x1FE1160 + mode * 8)
        check(prefix + "_name", image.read(string_rva, len(name) + 1).hex(),
              (name.encode("ascii") + b"\0").hex())
        check(prefix + "_case", u32(0x1290398 + mode * 4), case)
        check(prefix + "_constructor_call", call_target(call), constructor)
        if vtable is not None:
            # Confirm the constructor's RIP-relative LEA selects that vtable.
            lea = {0: 0x12815A5, 2: 0x1281498, 4: 0x1281434}[mode]
            encoded = image.read(lea, 7)
            check(prefix + "_vtable_opcode", encoded[:3].hex(), "488d05")
            check(prefix + "_vtable", lea + 7 + struct.unpack_from("<i", encoded, 3)[0], vtable)
    for mode, name, case, call, constructor in (
        (0, "gameplay", 0x1289253, 0x1289268, 0x12815D0),
        (3, "debug", 0x1289272, 0x128928B, 0x1281190),
    ):
        check(f"director_{mode}_{name}_case", u32(0x1289670 + mode * 4), case)
        check(f"director_{mode}_{name}_constructor", call_target(call), constructor)
    check("flying_update_vtable_slot", pointer_rva(0x1C80860 + 0x10), 0x1293790)
    check("following_update_vtable_slot", pointer_rva(0x1C803D0 + 0x10), 0x1294500)
    check("debug_update_vtable_slot", pointer_rva(0x1C807F8 + 8), 0x1291CB0)
    check("gameplay_update_vtable_slot", pointer_rva(0x1C809D0 + 8), 0x12954C0)
    return {
        "build": "86657.20.08.23.1800.d2_rc",
        "verified_at_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "Read-only native code and data bindings; no gameplay or input-isolation acceptance",
        "passed": all(row["passed"] for row in checks),
        "check_count": len(checks),
        "checks": checks,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--pid", type=int)
    source.add_argument("--snapshot", type=Path)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    with args.exe.open("rb") as stream:
        exe_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    if exe_hash != EXPECTED_EXE_SHA256:
        parser.error("Executable does not match the recovered build; no process reads attempted")
    image = LiveImage(args.pid, args.exe) if args.pid is not None else Snapshot(args.snapshot)
    try:
        result = verify(image, exe_hash)
    finally:
        image.close()
    result["source"] = "running_process" if args.pid is not None else "mapped_section_snapshot"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"{'PASS' if result['passed'] else 'FAIL'}: "
          f"{sum(row['passed'] for row in result['checks'])}/{result['check_count']} native camera binding checks")
    for row in result["checks"]:
        if not row["passed"]:
            print(json.dumps(row))
    print(result["scope"])
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
