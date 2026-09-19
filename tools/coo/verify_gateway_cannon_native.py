"""Read-only check of the pinned native force-accumulation boundary.

Run against the mapped-layout destiny2_unpacked.bin, not the packed executable.
No process access, package mutation, or code injection is performed.
"""
import argparse
from pathlib import Path
import struct


def verify(image: bytes) -> None:
    # Exact prologue checked again by the hook before attaching.
    expected = {
        0xD3B550: "405556574154415541564157488dac24",
        # D39B30 clears force/velocity and their apply flags for each target.
        0xD39FE3: "0f57f6",  # xorps xmm6,xmm6
        0xD3A008: "44896b246644896b210f113344886b230f117310",
        # rcx=component, rdx=target, r8d=region, xmm3=delta time; return unused.
        0xD3A030: "f3410f105f10488bd34863c6488bcf448b448378e807150000ffc63b73747ce0",
        # Applying force and velocity is conditional on the freshly reset flags.
        0xD3A058: "488bf044386b220f8435010000488bd3488bc8e8305b71ff",
        0xD3A19A: "44386b237426488b4620488d55a0488bce0f1080400200000f54057742e6000f5843100f2945a0e80aa372ff",
    }
    for rva, encoded in expected.items():
        data = bytes.fromhex(encoded)
        if image[rva:rva + len(data)] != data:
            raise ValueError(f"Native boundary mismatch at RVA {rva:08X}")
    target = 0xD3A049 + struct.unpack_from("<i", image, 0xD3A045)[0]
    if target != 0xD3B550:
        raise ValueError("Force accumulation call target changed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    verify(args.image.read_bytes())
    print("Gateway cannon native boundary: 6 byte fixtures and call ABI passed")
