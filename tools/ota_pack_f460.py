#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_pack_f460.py — HC32F460 报警板 A/B 槽镜像打包（生成 .otapkg）

背景：报警板是 A/B 双槽【无搬运】—— 两个槽的 App 是两份独立编译产物（链接基址不同），
      所以每个槽要打一个包，目标板按自己的非活动槽选对应包下发。

包格式（与 tools/ota_pack.py 完全一致，固件端 ota_transport_uart.c 解析）：
  [0:4]   MAGIC "OTA1"
  [4:8]   version  (uint32 LE)
  [8]     platform_id (1=HC32F460)
  [9]     app_id   (0=主固件)
  [10:14] payload_len (uint32 LE)
  [14:18] CRC32(payload) (uint32 LE, IEEE 0xEDB88320)
  [18:50] SHA256(payload)
  [50:82] HMAC-SHA256(key, [0:50]+payload)
  [82:]   payload = 【App 二进制本体】（槽尾元数据由设备端在收满后自行写入，
          其 CRC32 由 Flash 实际内容算出——所以包里不含元数据）

用法：
  python tools/ota_pack_f460.py --slot-a <A.hex|A.bin> --slot-b <B.hex|B.bin> \
         --version 0x01020304 --out dist
  # 也可只打一个槽：--slot-a 或 --slot-b 任给其一
"""
import argparse, binascii, hashlib, hmac, os, struct, sys, zlib

OTA_MAGIC       = b"OTA1"
OTA_HEADER_LEN  = 82
OTA_SIGN_OFF    = 50
DEMO_HMAC_KEY   = b"HC32F4A0_OTA_KEY_DEMO_20260814"
PLATFORM_F460   = 1
APP_ID_MAIN     = 0

# 槽基址（必须与 projects/source/ota_layout.h 一致）
SLOT_A_BASE     = 0x00008000
SLOT_B_BASE     = 0x00028000
SLOT_SIZE       = 0x00020000
TRAILER_SIZE    = 0x20
IMG_MAX         = SLOT_SIZE - TRAILER_SIZE

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def parse_hex(path):
    """Intel HEX -> (start_addr, bytes)，空隙补 0xFF"""
    data = bytearray()
    base = 0
    addr = None
    start = 0
    with open(path, "r") as f:
        for ln, line in enumerate(f, 1):
            line = line.strip()
            if not line or line[0] != ":":
                continue
            b = binascii.unhexlify(line[1:])
            if len(b) < 5:
                raise SystemExit(f"{path}:{ln}: bad hex record")
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
            if addr is None:
                addr = a
                start = a
            if a > addr + len(data):
                data.extend(b"\xff" * (a - (addr + len(data))))
            data.extend(b[4:4 + reclen])
            addr = a
    if addr is None:
        raise SystemExit(f"{path}: no data records")
    return start, bytes(data)


def load_image(path):
    """hex 或 bin -> (起始地址, 二进制)。bin 的起始地址由调用方按槽基址假定"""
    if path.lower().endswith(".hex"):
        return parse_hex(path)
    with open(path, "rb") as f:
        return None, f.read()


def build_pkg(payload, version, platform, app_id, key):
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    sha = hashlib.sha256(payload).digest()
    hdr = (OTA_MAGIC + struct.pack("<I", version) + bytes([platform, app_id])
           + struct.pack("<I", len(payload)) + struct.pack("<I", crc) + sha)
    assert len(hdr) == OTA_SIGN_OFF, f"header prefix must be {OTA_SIGN_OFF}B, got {len(hdr)}"
    sig = hmac.new(key, hdr + payload, hashlib.sha256).digest()
    hdr += sig
    assert len(hdr) == OTA_HEADER_LEN
    return hdr + payload


def do_slot(name, path, base, version, outdir, key):
    start, data = load_image(path)
    if start is not None and start != base:
        raise SystemExit(f"{name}: 起始地址 0x{start:08X} 与槽基址 0x{base:08X} 不符 —— "
                         f"是不是拿错了编译产物（A/B 两份产物不能互串）？")
    if len(data) > IMG_MAX:
        raise SystemExit(f"{name}: 镜像 {len(data)}B 超过上限 {IMG_MAX}B")
    pkg = build_pkg(data, version, PLATFORM_F460, APP_ID_MAIN, key)
    out = os.path.join(outdir, f"radar_{name}_v{version:08X}.otapkg")
    with open(out, "wb") as f:
        f.write(pkg)
    print(f"  {name}: 镜像 {len(data)}B  ->  {out}  包 {len(pkg)}B")
    print(f"        CRC32=0x{zlib.crc32(data) & 0xFFFFFFFF:08X}  SHA256={hashlib.sha256(data).hexdigest()[:16]}...")
    return out


def main():
    ap = argparse.ArgumentParser(description="HC32F460 报警板 A/B 槽镜像打包")
    ap.add_argument("--slot-a", default=None, help="槽 A 的 App 产物（.hex/.bin，链接基址 0x8000）")
    ap.add_argument("--slot-b", default=None, help="槽 B 的 App 产物（.hex/.bin，链接基址 0x28000）")
    ap.add_argument("--version", type=lambda x: int(x, 0), required=True, help="版本号，如 0x01020304")
    ap.add_argument("--out", default="dist", help="输出目录（默认 dist）")
    ap.add_argument("--key", default=None, help="HMAC 密钥文件；缺省用 demo key")
    a = ap.parse_args()

    key = DEMO_HMAC_KEY
    if a.key:
        with open(a.key, "rb") as f:
            key = f.read()
        print(f"HMAC key: {a.key} ({len(key)}B)")
    else:
        print("HMAC key: demo（与 tools/ota_pack.py 一致）")

    if not a.slot_a and not a.slot_b:
        raise SystemExit("至少要给 --slot-a 或 --slot-b 之一")

    os.makedirs(a.out, exist_ok=True)
    print(f"版本 0x{a.version:08X}   输出 {a.out}/")
    if a.slot_a:
        do_slot("slotA", a.slot_a, SLOT_A_BASE, a.version, a.out, key)
    if a.slot_b:
        do_slot("slotB", a.slot_b, SLOT_B_BASE, a.version, a.out, key)
    print("完成。目标板会用【非活动槽】对应的那个包。")


if __name__ == "__main__":
    main()
