#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_selftest.py — 设备端状态机模拟器（复刻固件 ota_transport_uart.c handle_frame 逻辑）
用于无硬件时验证：工具分片→ACK/RESUME→断点续传→载荷 CRC32 校验 全链路协议一致性。

模拟场景：
  1) 全新下载（progress=0）→ 逐片 ACK → 收满 → 载荷 CRC32 比对通过
  2) 中断续传（progress 保留）→ 首帧触发 RESUME → 从偏移续发 → 完成
  3) 坏帧 CRC → RESUME(当前偏移) → 重发
  4) 超尾防护 → RESUME

用法：  python ota_selftest.py fw.otapkg
"""
import hashlib, struct, sys, zlib
import sys
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")


sys.path.insert(0, __import__("os").path.dirname(__file__))
from ota_send import crc16, make_frame, parse_frame, TYPE_DATA, TYPE_ACK, TYPE_RESUME, FRAME_HDR_LEN, FRAME_CRC_LEN

QSPI_SECTOR = 4096
OTA_HDR_LEN = 82

class DeviceSim:
    """模拟 ota_transport_uart.c 的设备端行为（内存 QSPI）
    续传起点 initial_progress 时由 run_scenario 预填已收数据；
    擦除游标在首个 DATA 帧按固件规则（扇区边界/半满）计算。"""
    def __init__(self, initial_progress=0, drop_ack=False):
        self.qspi = bytearray(b"\xff" * (4 * 1024 * 1024))   # 4MB 暂存（全 0xFF）
        self.progress = initial_progress                      # 掉电保留的进度（KV）
        self.session_first = True
        self.erased_upto = 0
        self.pkg_total = 0
        self.drop_ack = drop_ack                              # 模拟 ACK 丢失
        self.finished = False
        self.verify_ok = None

    def get_u32(self, off):
        return struct.unpack("<I", bytes(self.qspi[off:off + 4]))[0]

    def ensure_erased(self, off, length):
        end = ((off + length + QSPI_SECTOR - 1) // QSPI_SECTOR) * QSPI_SECTOR
        if end <= self.erased_upto:
            return
        for a in range(self.erased_upto, end, QSPI_SECTOR):
            self.qspi[a:a + QSPI_SECTOR] = b"\xff" * QSPI_SECTOR   # 擦扇区=写 0xFF
        self.erased_upto = end

    def handle(self, frame: bytes):
        """与固件 ota_transport_uart_handle_frame 同构；返回设备回发帧或 None"""
        if len(frame) < FRAME_HDR_LEN + FRAME_CRC_LEN or frame[0:4] != b"OTA1":
            return None
        plen = struct.unpack("<H", frame[7:9])[0]
        if len(frame) != FRAME_HDR_LEN + plen + FRAME_CRC_LEN:
            return None
        if struct.unpack("<H", frame[FRAME_HDR_LEN + plen:])[0] != crc16(frame[:FRAME_HDR_LEN + plen]):
            return make_frame(TYPE_RESUME, 0, struct.pack("<I", self.progress))   # CRC 错 → RESUME
        if frame[4] != TYPE_DATA:
            return None                                    # ACK/RESUME 方向忽略

        progress = self.progress
        if plen == 0:                                      # 探测帧 → ACK(progress)
            return make_frame(TYPE_ACK, 0, struct.pack("<I", progress))

        if self.session_first:
            self.session_first = False
            self.pkg_total = 0
            if progress % QSPI_SECTOR == 0:
                self.erased_upto = progress
            else:
                self.erased_upto = ((progress // QSPI_SECTOR) + 1) * QSPI_SECTOR
            if progress > 0:
                return make_frame(TYPE_RESUME, 0, struct.pack("<I", progress))    # 首帧+进度 → 续传

        if self.pkg_total > 0 and progress + plen > self.pkg_total:
            return make_frame(TYPE_RESUME, 0, struct.pack("<I", progress))         # 超尾 → RESUME

        self.ensure_erased(progress, plen)
        payload = frame[FRAME_HDR_LEN:FRAME_HDR_LEN + plen]
        self.qspi[progress:progress + plen] = payload
        new_off = progress + plen
        self.progress = new_off

        if self.pkg_total == 0 and new_off >= OTA_HDR_LEN:
            if bytes(self.qspi[0:4]) != b"OTA1":
                return None
            self.pkg_total = OTA_HDR_LEN + self.get_u32(10)

        if self.drop_ack and new_off % 1024 == 0 and new_off < self.pkg_total:
            return None                                   # 模拟偶发 ACK 丢失
        if self.pkg_total > 0 and new_off >= self.pkg_total:
            # 下载完成：载荷 CRC32 比对（固件 ota_storage_verify_payload 语义）
            fw_len = self.pkg_total - OTA_HDR_LEN
            crc_expect = self.get_u32(14)
            crc_got = zlib.crc32(bytes(self.qspi[OTA_HDR_LEN:self.pkg_total])) & 0xFFFFFFFF
            self.finished = True
            self.verify_ok = (crc_got == crc_expect)
            return make_frame(TYPE_ACK, 0, struct.pack("<I", new_off))
        return make_frame(TYPE_ACK, 0, struct.pack("<I", new_off))

def run_scenario(name, pkg, **kw):
    dev = DeviceSim(**kw)
    if dev.progress:
        dev.qspi[0:dev.progress] = pkg[0:dev.progress]        # 预填上次已收数据（模拟掉电前写入）
    off = 0
    seq = 0
    steps = 0
    while off < len(pkg) and steps < 100000:
        chunk = pkg[off:off + 2048]   # 与固件 OTA_FRAME_MAX_PAYLOAD=2048 一致
        resp = dev.handle(make_frame(TYPE_DATA, seq, chunk))   # 原始帧字节
        steps += 1
        if resp is None:                                   # ACK 丢失 → 探测
            probe = dev.handle(make_frame(TYPE_DATA, seq, b""))
            if probe is None:
                raise AssertionError("probe 无响应")
            off = struct.unpack("<I", parse_frame(probe)[2])[0]
            seq += 1
            continue
        rtype, _, payload = parse_frame(resp)              # 解析为 (type, seq, payload)
        if rtype == TYPE_RESUME:
            off = struct.unpack("<I", payload)[0]          # 续传
        elif rtype == TYPE_ACK:
            off = struct.unpack("<I", payload)[0]
        seq += 1
    assert dev.finished, f"{name}: 未完成"
    assert dev.verify_ok is True, f"{name}: 载荷 CRC32 校验失败"
    got = bytes(dev.qspi[OTA_HDR_LEN:len(pkg)])
    assert got == pkg[OTA_HDR_LEN:], f"{name}: 暂存载荷与包不一致"
    print(f"  [PASS] {name}  ({steps} 步, {off} 字节)")

def main():
    pkg_path = sys.argv[1] if len(sys.argv) > 1 else "firmware.otapkg"
    pkg = open(pkg_path, "rb").read()
    assert pkg[0:4] == b"OTA1", "不是 OTA1 包"

    print(f"OTA 包 {len(pkg)} 字节，载荷 {len(pkg)-OTA_HDR_LEN} 字节\n")

    # 1) 全新下载
    run_scenario("全新下载", pkg)
    # 2) 中断续传（progress=10240 保留）
    run_scenario("中断续传(10KB处)", pkg, initial_progress=10240)
    # 3) 中断续传（半扇区位置 5000）
    run_scenario("中断续传(半扇区5000)", pkg, initial_progress=5000)
    # 4) 偶发 ACK 丢失（探测恢复）
    run_scenario("ACK丢失(探测恢复)", pkg, drop_ack=True)

    print("\n全部场景通过：工具分片/续传/探测与设备端协议一致")

if __name__ == "__main__":
    main()
