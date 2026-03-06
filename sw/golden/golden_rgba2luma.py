#!/usr/bin/env python3
"""
Golden model for rgba2luma_min.c

- Input pixels: in_pixels[i] = 0xAABBGGRR where
    RR = (i*3) & 0xFF
    GG = (i*5) & 0xFF
    BB = (i*7) & 0xFF
    AA = 0xAA
- Luma: y = (77*R + 150*G + 29*B) >> 8
- out_luma[i] is uint8_t
- sum = Σ out_luma[i] (uint32_t)

Optional: parse a Verilator TB log (sim_all.txt) and reconstruct out_luma[]
from OBI writes to a known base address using byte enables.
"""

import re
import sys
from typing import Dict, List, Tuple

NPIX = 16

def rgba2luma(p: int) -> int:
    r = (p >> 0) & 0xFF
    g = (p >> 8) & 0xFF
    b = (p >> 16) & 0xFF
    y = (77 * r + 150 * g + 29 * b) >> 8
    return y & 0xFF

def build_inputs() -> List[int]:
    in_pixels = []
    for i in range(NPIX):
        rr = (i * 3) & 0xFF
        gg = (i * 5) & 0xFF
        bb = (i * 7) & 0xFF
        p = (0xAA << 24) | (bb << 16) | (gg << 8) | (rr << 0)  # 0xAABBGGRR
        in_pixels.append(p)
    return in_pixels

def golden_outputs() -> Tuple[List[int], List[int], int]:
    in_pixels = build_inputs()
    out_luma = [rgba2luma(p) for p in in_pixels]
    s = sum(out_luma) & 0xFFFFFFFF
    return in_pixels, out_luma, s

# ---------- Optional log parsing to extract out_luma from OBI writes ----------

WR_RE = re.compile(
    r"^\[D \]\s+accept\s+addr=0x([0-9a-fA-F]+)\s+\(WR\)\s+wdata=0x([0-9a-fA-F]+)\s+be=0x([0-9a-fA-F]+)"
)

def apply_write(mem: Dict[int, int], addr: int, wdata: int, be: int) -> None:
    """
    OBI-style write: wdata is 32-bit, be is 4-bit byte enable.
    Byte lane j writes to address (addr + j) the byte (wdata >> (8*j)) & 0xFF.
    """
    for j in range(4):
        if (be >> j) & 1:
            mem[addr + j] = (wdata >> (8 * j)) & 0xFF

def extract_out_luma_from_log(log_path: str, out_base: int) -> List[int]:
    mem: Dict[int, int] = {}
    with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            m = WR_RE.match(line.strip())
            if not m:
                continue
            addr = int(m.group(1), 16)
            wdata = int(m.group(2), 16)
            be = int(m.group(3), 16)
            # Only track writes in a small window around out_luma
            if out_base <= addr < out_base + 64:  # 64B window is plenty
                apply_write(mem, addr, wdata, be)

    # Read 16 bytes from base
    out = []
    for i in range(NPIX):
        a = out_base + i
        if a not in mem:
            raise RuntimeError(f"Missing byte at 0x{a:08x} in log; "
                               f"maybe compiler stored differently or base address is wrong.")
        out.append(mem[a])
    return out

def main() -> None:
    in_pixels, out_luma, s = golden_outputs()

    print("Golden model (matches rgba2luma_min.c):")
    print("in_pixels (0xAABBGGRR):")
    for i, p in enumerate(in_pixels):
        print(f"  [{i:2d}] 0x{p:08x}")
    print("out_luma (uint8):")
    for i, y in enumerate(out_luma):
        print(f"  [{i:2d}] {y:3d} (0x{y:02x})")
    print(f"sum = {s} (0x{s:08x})")
    print(f'Expected UART snippet: [rgba2luma] cycles=00000000 sum={s:08x}')

    # Optional: verify against a sim log if provided
    if len(sys.argv) == 3:
        log_path = sys.argv[1]
        out_base = int(sys.argv[2], 16)
        extracted = extract_out_luma_from_log(log_path, out_base)
        ok = extracted == out_luma
        print("\nExtracted out_luma from log:")
        for i, y in enumerate(extracted):
            print(f"  [{i:2d}] {y:3d} (0x{y:02x})")
        print("\nCOMPARE:", "PASS ✅" if ok else "FAIL ❌")
        if not ok:
            print("First mismatch (index, golden, extracted):")
            for i in range(NPIX):
                if out_luma[i] != extracted[i]:
                    print(i, out_luma[i], extracted[i])
                    break

if __name__ == "__main__":
    main()