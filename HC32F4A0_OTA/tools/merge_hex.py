#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
merge_hex.py — 合并多个 Intel HEX 为单个烧录文件（F4A0 双区布局）

用途：生产烧录用。F4A0 Phase 4 布局下需分别烧 boot.hex(0x0) 与 app hex(0x10000)，
本工具合并为一个 .hex，配合烧录器按地址一次写入。

用法：
  python merge_hex.py boot.hex app.hex -o merged.hex
  python merge_hex.py --list boot.hex app.hex     # 仅查看各文件地址范围/大小

特性：
  - 自动检测地址重叠（报错）
  - 空隙保留（不补 0xFF，保持原样）
  - 支持 Extended Linear Address (0x04) 记录
"""
import argparse, binascii, struct, sys

def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def parse_hex(path):
    """Intel HEX -> {addr: bytearray} 分段字典 + (min_addr, max_addr)"""
    segs = {}
    base = 0
    for ln, line in enumerate(open(path, "r"), 1):
        line = line.strip()
        if not line or line[0] != ':':
            continue
        b = binascii.unhexlify(line[1:])
        if len(b) < 5:
            raise SystemExit(f"{path}:{ln}: bad record")
        reclen, recaddr, rectype = b[0], struct.unpack(">H", b[1:3])[0], b[3]
        if rectype == 0x04:
            base = struct.unpack(">H", b[4:6])[0] << 16
            continue
        if rectype == 0x01:
            break
        if rectype != 0x00:
            continue
        if reclen + 5 != len(b):
            raise SystemExit(f"{path}:{ln}: bad length")
        a = base + recaddr
        segs.setdefault(a, bytearray()).extend(b[4:4 + reclen])
    if not segs:
        raise SystemExit(f"{path}: no data")
    # 合并相邻段
    merged = {}
    for a in sorted(segs):
        data = segs[a]
        if merged and a == next(reversed(merged)) + len(merged[next(reversed(merged))]):
            prev_a = next(reversed(merged))
            merged[prev_a].extend(data)
        else:
            merged[a] = data
    addrs = sorted(merged)
    return merged, addrs[0], addrs[-1] + len(merged[addrs[-1]]) - 1

def _rec(rec):
    cs = (0x100 - (sum(rec) & 0xFF)) & 0xFF
    return ":" + binascii.hexlify(rec + bytes([cs])).decode().upper()

def write_hex(path, segs):
    lines = []
    cur_base = None
    addrs = sorted(segs)
    for a in addrs:
        data = segs[a]
        # 按 32 字节记录输出；>64KB 用 Extended Linear Address(0x04)
        for off in range(0, len(data), 16):
            chunk = data[off:off + 16]
            addr = a + off
            base = addr >> 16
            if base != cur_base:
                lines.append(_rec(bytes([2]) + struct.pack(">H", 0) + bytes([4]) + struct.pack(">H", base)))
                cur_base = base
            rec = bytes([len(chunk)]) + struct.pack(">H", addr & 0xFFFF) + bytes([0]) + bytes(chunk)
            lines.append(_rec(rec))
    # Start Linear Address (type 0x05, at END like Keil): entry = Reset vector of first segment
    entry = 0
    if addrs:
        first = segs[addrs[0]]
        if len(first) >= 8:
            entry = struct.unpack("<I", bytes(first[4:8]))[0]
    lines.append(_rec(bytes([4]) + struct.pack(">H", 0) + bytes([5]) + struct.pack(">I", entry)))
    lines.append(":00000001FF")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

def main():
    ep()
    ap = argparse.ArgumentParser(description="合并 Intel HEX 烧录文件")
    ap.add_argument("inputs", nargs="+", help="输入 hex（按顺序合并，地址自动定位）")
    ap.add_argument("-o", "--output", default="merged.hex")
    ap.add_argument("--list", action="store_true", help="仅列出各文件地址范围")
    a = ap.parse_args()

    all_segs = {}
    for f in a.inputs:
        segs, amin, amax = parse_hex(f)
        if a.list:
            print(f"{f}: 0x{amin:X}-0x{amax:X} ({amax-amin+1} bytes)")
            continue
        for addr, data in segs.items():
            if addr in all_segs or any(addr < s + len(d) for s, d in all_segs.items()):
                raise SystemExit(f"地址重叠: {f} @0x{addr:X}")
        for addr, data in segs.items():
            all_segs[addr] = data

    if a.list:
        return
    write_hex(a.output, all_segs)
    total = sum(len(d) for d in all_segs.values())
    addrs = sorted(all_segs)
    print(f"合并完成: {a.output} ({total} bytes, {len(all_segs)} 段)")
    print(f"  范围: 0x{addrs[0]:X} - 0x{addrs[-1]+len(all_segs[addrs[-1]])-1:X}")

if __name__ == "__main__":
    main()
