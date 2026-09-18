#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_send.py — 本地 OTA 串口发送器（"OTA1" 帧协议 + 0x52 断点续传）

帧格式（与固件端 ota_transport_uart.c 一致）：
  [0:4] "OTA1" | [4] type | [5:7] seq(16LE) | [7:9] len(16LE) | [9:9+N] payload | [末2] CRC16-CCITT-FALSE(LE)
  type: 0x50=DATA(上位机→设备)  0x51=ACK(设备→上位机)  0x52=RESUME(设备→上位机)
  ACK/RESUME payload: 4B LE 偏移（统一 OTA 包内绝对字节）

断点续传：设备已有进度时首帧回 0x52(offset)；帧 CRC 错/超尾也回 0x52。
上位机收到 0x52/0x51 后从 offset 续发；超时用 len=0 探测帧查询设备进度。

用法：
  python ota_send.py fw.otapkg --port COM3 --baud 115200
  python ota_send.py fw.otapkg --port COM3 --offset 0      # 强制从头
"""
import argparse, struct, sys, time
import sys
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

try:
    import serial
except ImportError:
    sys.exit("需要 pyserial:  pip install pyserial")

OTA_MAGIC        = b"OTA1"
TYPE_DATA, TYPE_ACK, TYPE_RESUME = 0x50, 0x51, 0x52
FRAME_HDR_LEN    = 9
FRAME_CRC_LEN    = 2
MAX_PAYLOAD      = 4096   # 与固件 ota_frame.h OTA_FRAME_MAX_PAYLOAD 一致（v9.81h 2048->4096 串口提速）
HDR_LEN          = 82          # 统一 OTA 包头长
CRC_INIT         = 0xFFFF

# ---------- CRC16-CCITT-FALSE（与固件端逐位算法一致） ----------
def crc16(data: bytes) -> int:
    crc = CRC_INIT
    for byte in data:
        crc ^= (byte << 8) & 0xFFFF
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc

def make_frame(ftype: int, seq: int, payload: bytes) -> bytes:
    hdr = OTA_MAGIC + bytes([ftype]) + struct.pack("<H", seq) + struct.pack("<H", len(payload))
    body = hdr + payload
    return body + struct.pack("<H", crc16(body))

def parse_frame(buf: bytes):
    """解析一帧，返回 (type, seq, payload) 或 None"""
    if len(buf) < FRAME_HDR_LEN + FRAME_CRC_LEN:
        return None
    if buf[0:4] != OTA_MAGIC:
        return None
    plen = struct.unpack("<H", buf[7:9])[0]
    if len(buf) != FRAME_HDR_LEN + plen + FRAME_CRC_LEN:
        return None
    body = buf[:FRAME_HDR_LEN + plen]
    if struct.unpack("<H", buf[FRAME_HDR_LEN + plen:])[0] != crc16(body):
        return None
    return buf[4], struct.unpack("<H", buf[5:7])[0], buf[FRAME_HDR_LEN:FRAME_HDR_LEN + plen]

class OtaSender:
    def __init__(self, port, baud, pkg, start_offset=None, max_payload=MAX_PAYLOAD):
        self.ser = serial.Serial(port, baud, timeout=1.0, write_timeout=2.0)
        self.pkg = pkg
        self.total = len(pkg)
        self.start_offset = start_offset
        self.max_payload = min(max_payload, MAX_PAYLOAD)   # 设备上限保护
        self.offset = 0 if start_offset is None else min(start_offset, self.total)
        self.seq = 0
        self.t0 = time.time()
        self.trace_buf = b""   # 捕获设备 TRACE 文本（与 ACK 共用一条线）
        self.rawlog = None    # --rawlog 时记录设备→PC 原始字节

    def read_frame(self, timeout):
        """读一帧（按 magic 同步），返回 (type, seq, payload) 或 None；非帧字节作为设备 TRACE 捕获"""
        self.ser.timeout = timeout
        end = time.time() + timeout
        while time.time() < end:
            b = self.ser.read(1)
            if not b:
                return None
            if self.rawlog:
                self.rawlog.write(b.hex() + " ")
                self.rawlog.flush()
            if b != b"O":
                # 设备 TRACE（printf 与 OTA 帧共用 USART1）：按行捕获打印
                if b == b"\n":
                    line = self.trace_buf.decode("utf-8", errors="replace").strip()
                    self.trace_buf = b""
                    if line:
                        print("  [trace]", line)
                else:
                    self.trace_buf += b
                continue                      # 重同步
            rest = self.ser.read(8)
            if len(rest) < 8:
                return None
            head = b + rest
            if head[1:4] != b"TA1":
                self.trace_buf += head
                continue
            plen = struct.unpack("<H", head[7:9])[0]
            tail = self.ser.read(plen + FRAME_CRC_LEN)
            if len(tail) < plen + FRAME_CRC_LEN:
                return None
            return parse_frame(head + tail)
        return None

    def send_probe(self):
        """len=0 DATA 探测：查询设备当前进度（设备回 ACK(progress)）"""
        self.ser.write(make_frame(TYPE_DATA, self.seq, b""))
        return self.read_frame(2.0)

    def run(self, progress_cb=None, verbose=False):
        total = self.total
        # v9.80: 会话首帧始终发包头帧（82B OTA1）——设备据此做包一致性握手：
        #   一致 → 回 RESUME(进度) 续传；不一致（旧包残留/包变更）→ 清进度回 ACK(82) 全新下载。
        # 显式 --offset 时跳过握手，直接按指定偏移发送。
        if self.start_offset is None:
            self.offset = 0
            self.ser.write(make_frame(TYPE_DATA, self.seq, self.pkg[:82]))
            self.ser.flush()
            self.seq += 1
            resp = self.read_frame(4.0)
            if resp is None:
                print("!! 包头帧无响应，从 0 重试")
            elif len(resp[2]) >= 4:
                dev_off = struct.unpack("<I", resp[2])[0]
                if resp[0] == TYPE_RESUME:
                    self.offset = min(dev_off, total)
                    if self.offset > 0:
                        print(f"  [续传] 设备进度 {self.offset} 字节，从该偏移继续")
                elif resp[0] == TYPE_ACK:
                    self.offset = dev_off if dev_off < total else 0
                    print(f"  [全新] 设备暂存已就绪（包头写入 @{self.offset}），开始下载")
        t_frame = time.time()
        while self.offset < total:
            chunk = self.pkg[self.offset:self.offset + self.max_payload]
            self.ser.write(make_frame(TYPE_DATA, self.seq, chunk))
            resp = self.read_frame(3.0)
            now = time.time()
            dt = now - t_frame
            t_frame = now
            resp_tag = "?"

            if resp is None:
                # 超时 → 探测设备进度（可能 ACK 丢失/设备擦扇区慢）
                probe = self.send_probe()
                if probe is None:
                    print("!! 超时且探测无响应，重试当前片")
                    continue
                ftype, _, off = probe[0], probe[1], struct.unpack("<I", probe[2])[0] if len(probe[2]) >= 4 else self.offset
                if ftype == TYPE_ACK:
                    self.offset = off
                elif ftype == TYPE_RESUME:
                    self.offset = off
                self.seq += 1
                resp_tag = "TIMEOUT+probe"
            elif resp[0] == TYPE_ACK:
                off = struct.unpack("<I", resp[2])[0]
                if off > self.offset:
                    self.offset = off          # 正常推进（ACK 带已写偏移）
                else:
                    self.offset = off          # 设备进度异常时跟随设备
                self.seq += 1
                resp_tag = "ACK"
            elif resp[0] == TYPE_RESUME:
                off = struct.unpack("<I", resp[2])[0]
                self.offset = off              # 续传：从设备偏移继续
                self.seq += 1
                resp_tag = "RESUME"

            if verbose:
                print(f"  [v] off={self.offset:6d}/{total} dt={dt:6.2f}s resp={resp_tag}")
            if progress_cb:
                progress_cb(self.offset, total)

        # 进度已满：设备可能因断点续传/重跑未触发 finish，发探测帧兜底
        if self.offset >= total:
            self.send_probe()

    def close(self):
        self.ser.close()

def main():
    ap = argparse.ArgumentParser(description="本地 OTA 串口发送器（OTA1 帧 + 断点续传）")
    ap.add_argument("pkg", help=".otapkg 文件")
    ap.add_argument("--port", required=True, help="串口，如 COM3")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--offset", type=lambda x: int(x, 0), default=None, help="强制从指定偏移开始")
    ap.add_argument("--payload", type=int, default=MAX_PAYLOAD, help=f"帧载荷字节数（≤{MAX_PAYLOAD}，默认 {MAX_PAYLOAD}）")
    ap.add_argument("--verbose", action="store_true", help="逐帧打印耗时/响应类型（诊断用）")
    ap.add_argument("--rawlog", default=None, help="设备→PC 原始字节记录文件（hex，诊断用）")
    a = ap.parse_args()

    pkg = open(a.pkg, "rb").read()
    if pkg[0:4] != b"OTA1":
        sys.exit("不是有效的 OTA 包（缺少 OTA1 魔数）")

    s = OtaSender(a.port, a.baud, pkg, a.offset, a.payload)
    if a.rawlog:
        s.rawlog = open(a.rawlog, "w", encoding="ascii")
    print(f"verbose: {'ON' if a.verbose else 'OFF'}")
    print(f"OTA 包: {len(pkg)} 字节（载荷 {len(pkg)-HDR_LEN}），端口 {a.port}@{a.baud}")
    print(f"起始偏移: {s.offset}  {'（续传）' if s.offset > 0 else ''}")

    last_pct = -1
    def cb(off, total):
        nonlocal last_pct
        pct = off * 100 // total
        if pct != last_pct:
            last_pct = pct
            el = time.time() - s.t0
            rate = off / el / 1024 if el > 0 else 0
            print(f"  {pct:3d}%  {off}/{total}  ({rate:.1f} KB/s)")
    try:
        s.run(progress_cb=cb, verbose=a.verbose)
        print(f"完成：全部 {s.total} 字节已发送，设备正在验签并重启...")
    except KeyboardInterrupt:
        print("\n已中断（Ctrl+C）。设备进度已保留，重跑本命令即可续传。")
    finally:
        if s.rawlog:
            s.rawlog.close()
        s.close()

if __name__ == "__main__":
    main()
