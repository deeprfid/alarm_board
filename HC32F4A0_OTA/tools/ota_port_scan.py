#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_port_scan.py — 扫描所有串口 x 波特率，定位设备 OTA/命令口

对每个 (COM 口, 波特率) 组合：
  1) 发 OTA1 len=0 探测帧，等 1.5s —— 设备 OTA 通道监听 -> 回 ACK/RESUME
  2) 发 'A'，等 0.8s —— 任何回显/响应都算"有数据"（可能是模块数据/调试输出）

用法：
  python ota_port_scan.py                      # 自动枚举所有 COM 口
  python ota_port_scan.py --ports COM3,COM6    # 指定口
"""
import argparse, os, struct, sys, time

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from ota_send import make_frame, parse_frame, TYPE_DATA, TYPE_ACK, TYPE_RESUME  # noqa: E402

BAUDS = [115200, 921600, 9600, 460800, 57600, 38400, 19200, 230400]


def read_any(ser, seconds):
    """收任意字节，返回 bytes；超时返回 b''"""
    ser.timeout = 0.3
    out = b""
    end = time.time() + seconds
    while time.time() < end:
        b = ser.read(64)
        if b:
            out += b
    return out


def try_ota_probe(ser):
    """发 OTA1 探测帧，等 1.5s，返回 (ACK/RESUME, offset) 或 None"""
    ser.reset_input_buffer()
    ser.write(make_frame(TYPE_DATA, 0, b""))
    ser.timeout = 1.5
    end = time.time() + 1.5
    while time.time() < end:
        b = ser.read(1)
        if not b:
            return None
        if b != b"O":
            continue
        rest = ser.read(8)
        if len(rest) < 8:
            return None
        head = b + rest
        if head[1:4] != b"TA1":
            continue
        plen = struct.unpack("<H", head[7:9])[0]
        tail = ser.read(plen + 2)
        if len(tail) < plen + 2:
            return None
        fr = parse_frame(head + tail)
        if fr:
            return fr
    return None


def scan_port(port):
    import serial
    print(f"===== {port} =====")
    for baud in BAUDS:
        try:
            ser = serial.Serial(port, baud, timeout=0.3, write_timeout=1.0)
        except Exception as e:
            print(f"  {port}@{baud:<7} 打不开: {e}")
            return
        tag = "无响应"
        try:
            # 先清空输入缓冲（可能有模块/日志数据）
            ser.reset_input_buffer()
            fr = try_ota_probe(ser)
            if fr is not None:
                ftype = {TYPE_ACK: "ACK", TYPE_RESUME: "RESUME"}.get(fr[0], hex(fr[0]))
                off = struct.unpack("<I", fr[2])[0] if len(fr[2]) >= 4 else 0
                tag = f"OTA {ftype} off={off}  <== 设备 OTA 通道在此!"
            else:
                data = read_any(ser, 0.8)
                if data:
                    shown = " ".join(f"{x:02X}" for x in data[:12])
                    tag = f"有数据 {len(data)}B: {shown} ..."
                else:
                    tag = "无响应"
        finally:
            ser.close()
        print(f"  {port}@{baud:<7} {tag}")


def main():
    ap = argparse.ArgumentParser(description="串口 x 波特率扫描（定位设备 OTA 口）")
    ap.add_argument("--ports", default=None, help="逗号分隔，如 COM3,COM6；默认自动枚举")
    a = ap.parse_args()
    if a.ports:
        ports = [p.strip() for p in a.ports.split(",") if p.strip()]
    else:
        import serial.tools.list_ports
        ports = sorted(p.device for p in serial.tools.list_ports.comports())
    if not ports:
        sys.exit("没有发现串口")
    print(f"扫描 {len(ports)} 个口 x {len(BAUDS)} 个波特率（每个约 3s，共约 {len(ports)*len(BAUDS)*3//60} 分钟）")
    for p in ports:
        scan_port(p)


if __name__ == "__main__":
    main()
