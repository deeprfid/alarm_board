#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
merge_f460_image.py —— 把 Boot + App 槽 A/B 合成【一个整片 bin】，产线一次烧录。

背景（见 docs/ota_boot_design.md §3 与 ota_layout.h）：

    Boot      0x00000000 - 0x00007FFF   32KB   （含 ICG@0x400、向量表）
    槽 A      0x00008000 - 0x00027FFF  128KB
    槽 B      0x00028000 - 0x00047FFF  128KB
    保留      0x00048000 - 0x0007DFFF  ~216KB
    标志区    0x0007E000 - 0x0007FFFF    8KB   （双份各 4KB）

【标志区刻意留 0xFF（= 擦除态）】这样烧完首次上电，Boot 读标志失败 -> 走兜底路径
-> 按 BOOT_DEFAULT_SLOT 优先选槽（当前 boot_ota.c 里 = OTA_SLOT_B）。
不要在这个 bin 里预写标志：那会绕过兜底路径、也让「首次烧录不写标志」这条纪律失效。

用法（不给参数就用仓库里的默认产物路径）：

    python tools/merge_f460_image.py
    python tools/merge_f460_image.py --build release
    python tools/merge_f460_image.py --slot-a-only          # 只含 Boot + 槽 A
    python tools/merge_f460_image.py --out dist/xxx.bin
"""
import argparse
import os
import sys

FLASH_SIZE = 0x00080000
BOOT_BASE, BOOT_SIZE = 0x00000000, 0x00008000
SLOT_SIZE = 0x00020000
SLOT_BASE = {0: 0x00008000, 1: 0x00028000}
FLAG_BASE, FLAG_SIZE = 0x0007E000, 0x00002000

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_hex(path):
    """Intel HEX -> {addr: byte}。支持 04/02 扩展地址记录。"""
    mem = {}
    base = 0
    with open(path, "r") as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line or not line.startswith(":"):
                continue
            try:
                n = int(line[1:3], 16)
                off = int(line[3:7], 16)
                typ = int(line[7:9], 16)
                data = bytes.fromhex(line[9:9 + n * 2])
            except ValueError:
                raise SystemExit("HEX 解析失败: %s:%d" % (path, lineno))
            if typ == 0x00:
                for i, b in enumerate(data):
                    mem[base + off + i] = b
            elif typ == 0x04:
                base = int.from_bytes(data, "big") << 16
            elif typ == 0x02:
                base = int.from_bytes(data, "big") << 4
            # 01=EOF / 03/05=起始地址，忽略
    if not mem:
        raise SystemExit("HEX 是空的: %s" % path)
    return mem


def span(mem):
    return min(mem), max(mem)


def place(image, base, size, label):
    """把 image 放到 base，校验不越界。返回 (lo, hi)"""
    lo, hi = span(image)
    if lo < base:
        raise SystemExit("%s: 起始 0x%05X 早于分区基址 0x%05X" % (label, lo, base))
    if hi >= base + size:
        raise SystemExit("%s: 结束 0x%05X 超出分区 [0x%05X,0x%05X) —— 放不下"
                         % (label, hi, base, base + size))
    return lo, hi


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", choices=["debug", "release"], default="release",
                    help="用哪套产物（默认 release）")
    ap.add_argument("--boot", default=None, help="Boot hex 路径")
    ap.add_argument("--slot-a", default=None, help="槽 A hex 路径")
    ap.add_argument("--slot-b", default=None, help="槽 B hex 路径")
    ap.add_argument("--slot-a-only", action="store_true", help="只合成 Boot + 槽 A")
    ap.add_argument("--out", default=None, help="输出 bin 路径")
    args = ap.parse_args()

    b = args.build
    boot_hex = args.boot or os.path.join(
        ROOT, "Radar_V4.2_2026_0425_MOS/projects/boot/MDK/output", b, "iap_boot.hex")
    a_hex = args.slot_a or os.path.join(
        ROOT, "Radar_V4.2_2026_0425_MOS/projects/MDK/output",
        b, "usart_uart_dma.hex")
    b_hex = args.slot_b or os.path.join(
        ROOT, "Radar_V4.2_2026_0425_MOS/projects/MDK/output",
        "slotb_" + b, "usart_uart_dma_b.hex")

    for p in (boot_hex, a_hex):
        if not os.path.isfile(p):
            raise SystemExit("找不到: %s" % p)

    flash = bytearray([0xFF] * FLASH_SIZE)
    rows = []

    boot = read_hex(boot_hex)
    place(boot, BOOT_BASE, BOOT_SIZE, "Boot")
    for a, v in boot.items():
        flash[a] = v
    rows.append(("Boot", BOOT_BASE, boot_hex, min(boot), max(boot)))

    # ICG 必须落在 Boot 里（0x400，32B）；这是「Boot 独占 ICG」的硬校验
    icg = [a for a in range(0x400, 0x420) if a in boot]
    if len(icg) != 32:
        raise SystemExit("Boot 里没有完整的 ICG（0x400 处应有 32 字节，实测 %d）" % len(icg))

    a_img = read_hex(a_hex)
    place(a_img, SLOT_BASE[0], SLOT_SIZE, "槽 A")
    for a, v in a_img.items():
        flash[a] = v
    rows.append(("槽 A", SLOT_BASE[0], a_hex, min(a_img), max(a_img)))

    if not args.slot_a_only:
        if not os.path.isfile(b_hex):
            raise SystemExit("找不到槽 B 产物: %s（要只合成 A 就加 --slot-a-only）" % b_hex)
        b_img = read_hex(b_hex)
        place(b_img, SLOT_BASE[1], SLOT_SIZE, "槽 B")
        for a, v in b_img.items():
            flash[a] = v
        rows.append(("槽 B", SLOT_BASE[1], b_hex, min(b_img), max(b_img)))

    suffix = "boot_A" if args.slot_a_only else "boot_A_B"
    out = args.out or os.path.join(ROOT, "dist", "radar_full_%s_%s.bin" % (suffix, b))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(flash)

    print("合成整片镜像: %s" % out)
    print("  总长 %d 字节 (0x%X) = 整片 %dKB" % (len(flash), len(flash), FLASH_SIZE // 1024))
    print()
    print("  分区        基址       来源产物的地址范围")
    for label, base, src, lo, hi in rows:
        print("  %-6s 0x%05X   %-58s 0x%05X-0x%05X (%d B)"
              % (label, base, os.path.relpath(src, ROOT), lo, hi, hi - lo + 1))
    print("  标志区  0x%05X   %s" % (FLAG_BASE, "留 0xFF（擦除态）"))
    print()
    print("  ICG@0x400 32B: 已确认在 Boot 内")
    print("  首次上电: 标志无效 -> Boot 走兜底路径 -> 按 BOOT_DEFAULT_SLOT 选槽")
    return 0


if __name__ == "__main__":
    sys.exit(main())
