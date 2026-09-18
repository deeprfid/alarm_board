#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fw_size_monitor.py — 固件体积监控（验收标准 8：F4A0 超 0.9MB 预警）

从 Keil .map 解析固件占用（Total ROM Size），对比阈值输出状态：
  - 正常（< 预警阈值）     → 退出码 0
  - 预警（≥ 预警阈值）     → 退出码 1（仍可编译，需关注）
  - 超上限（≥ App 区硬上限）→ 退出码 2（链接已失败/危险）

F4A0 Phase 4 布局：App 区 0x10000-0xEFFFF（896KB 硬上限）
  预警阈值 = 0.85 × 896KB ≈ 762KB（计划"超 0.9MB 预警"按 App 区折算）

用法：
  python fw_size_monitor.py --map output/firmware.map
  python fw_size_monitor.py --map output/firmware.map --platform 2 --warn 780 --max 896
"""
import argparse, re, sys

def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def parse_map(map_path):
    """返回 (rom_size_bytes, detail)"""
    with open(map_path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    m = re.search(r"Total ROM Size[^\r\n]*?(\d+)\s*\([^)]*\)", text)
    if not m:
        raise SystemExit(f"{map_path}: 未找到 Total ROM Size")
    return int(m.group(1)), "Total ROM Size"

def main():
    ep()
    ap = argparse.ArgumentParser(description="固件体积监控")
    ap.add_argument("--map", required=True, help="Keil .map 文件")
    ap.add_argument("--platform", type=int, default=2, help="2=F4A0（默认）")
    ap.add_argument("--warn", type=float, default=None, help="预警阈值 KB")
    ap.add_argument("--max", type=float, default=None, help="硬上限 KB")
    a = ap.parse_args()

    # 默认阈值（F4A0 App 区 896KB）
    max_kb = a.max if a.max is not None else 896.0
    warn_kb = a.warn if a.warn is not None else 0.85 * max_kb

    rom, src = parse_map(a.map)
    rom_kb = rom / 1024.0
    used_kb = max_kb - rom_kb  # 剩余空间

    print(f"固件占用: {rom_kb:.1f} KB（{src} = {rom} B）")
    print(f"App 区硬上限: {max_kb:.1f} KB，预警阈值: {warn_kb:.1f} KB")
    print(f"剩余空间: {used_kb:.1f} KB")

    if rom_kb >= max_kb:
        print(f"[超上限] 固件 {rom_kb:.1f}KB ≥ {max_kb:.1f}KB —— 链接将失败/危险")
        return 2
    if rom_kb >= warn_kb:
        print(f"[预警] 固件 {rom_kb:.1f}KB ≥ {warn_kb:.1f}KB —— 请关注体积增长")
        return 1
    print(f"[正常] {rom_kb:.1f}KB < {warn_kb:.1f}KB")
    return 0

if __name__ == "__main__":
    sys.exit(main())
