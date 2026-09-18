#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_uart_upgrade.py — H3 本地串口升级小工具（探测 + 升级 + 诊断 + 日志监视）

用法：
  python ota_uart_upgrade.py --port COM3 [--baud 115200] [--pkg server_data/firmware/fw_acc.otapkg]
  python ota_uart_upgrade.py --port COM3 --probe-only
  python ota_uart_upgrade.py --port COM3 --monitor     # 探测/升级后进入日志监视模式
"""
import argparse, os, struct, sys, time

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from ota_send import OtaSender, make_frame, parse_frame, TYPE_DATA, TYPE_ACK, TYPE_RESUME, HDR_LEN  # noqa: E402


_trace_buf = b""
def read_frame_raw(ser, timeout, verbose=False):
    """读一帧（按 magic 同步）；非帧字节按行捕获打印（设备 TRACE 与 ACK 共用一条线）"""
    global _trace_buf
    ser.timeout = timeout
    end = time.time() + timeout
    while time.time() < end:
        b = ser.read(1)
        if not b:
            return None
        if b != b"O":
            if verbose:
                if b == b"\n":
                    line = _trace_buf.decode("utf-8", errors="replace").strip()
                    _trace_buf = b""
                    if line:
                        print("  [trace]", line)
                else:
                    _trace_buf += b
            continue
        rest = ser.read(8)
        if len(rest) < 8:
            return None
        head = b + rest
        if head[1:4] != b"TA1":
            if verbose:
                _trace_buf += head
            continue
        plen = struct.unpack("<H", head[7:9])[0]
        tail = ser.read(plen + 2)
        if len(tail) < plen + 2:
            return None
        return parse_frame(head + tail)
    return None


def probe(port, baud, attempts, read_to, verbose=False):
    import serial
    try:
        ser = serial.Serial(port, baud, timeout=1.0, write_timeout=2.0)
    except Exception as e:
        print(f"[探测] 打不开串口 {port}: {e}")
        print("   检查：串口被占用（关掉串口助手/监视器）、USB-TTL 驱动、COM 号。")
        return None
    try:
        for i in range(1, attempts + 1):
            ser.reset_input_buffer()
            ser.write(make_frame(TYPE_DATA, 0, b""))
            print(f"  [探测 {i}/{attempts}] 发送 len=0 探测帧，等待 {read_to}s ...")
            resp = read_frame_raw(ser, read_to, verbose)
            if resp is not None:
                return resp
    finally:
        ser.close()
    return None


def monitor_ser(ser, port, baud):
    print(f"[监视] 正在读取 {port}@{baud} 设备日志（Ctrl+C 退出）...")
    buf = b""
    nl = chr(10).encode()
    cr = chr(13).encode()
    try:
        while True:
            chunk = ser.read(256)
            if chunk:
                buf += chunk
                while nl in buf:
                    line, buf = buf.split(nl, 1)
                    print(line.decode("utf-8", errors="replace").rstrip(cr.decode()))
    except KeyboardInterrupt:
        print()
        print("[监视] 已退出")


def monitor(port, baud):
    import serial
    print(f"[监视] 正在读取 {port}@{baud} 设备日志（Ctrl+C 退出）...")
    ser = serial.Serial(port, baud, timeout=1.0)
    buf = b""
    nl = chr(10).encode()
    cr = chr(13).encode()
    try:
        while True:
            chunk = ser.read(256)
            if chunk:
                buf += chunk
                while nl in buf:
                    line, buf = buf.split(nl, 1)
                    print(line.decode("utf-8", errors="replace").rstrip(cr.decode()))
    except KeyboardInterrupt:
        print()
        print("[监视] 已退出")
    finally:
        ser.close()


def diagnose(port, baud):
    print()
    print("========== 诊断：设备未响应 OTA 帧 ==========")
    print("1. 时序：send_func 要等模块初始化（最多约 10s）才进主循环处理 OTA 帧；")
    print("   刚复位/刚切模式后请加大 --attempts（如 10）再试。")
    print("2. 用 --monitor 看设备日志：确认是否有 'OTA: send_func enter main loop'。")
    print("3. 确认 USB-TTL 接的是设备命令口（USART1/CH340，PB0/PB1），波特率 115200；")
    print("   TX/RX 交叉、共地。")
    print("4. 若确认物理链路但始终无响应：暂存区可能有跨版本残留（固件 v9.5 起启动时")
    print("   会自动清不匹配版本），或 send_func 被 gIsModAPICtrl 卡住。")
    print("==============================================")


def main():
    ap = argparse.ArgumentParser(description="H3 本地串口 OTA（探测+升级+诊断+监视）")
    ap.add_argument("--port", default="COM3", help="串口，默认 COM3")
    ap.add_argument("--baud", type=int, default=115200, help="波特率，默认 115200")
    ap.add_argument("--pkg", default=os.path.join("server_data", "firmware", "fw_acc.otapkg"),
                    help="OTA 包路径（相对 tools/ 或绝对路径）")
    ap.add_argument("--attempts", type=int, default=6, help="探测重试次数，默认 6")
    ap.add_argument("--timeout", type=float, default=4.0, help="每次探测等待秒数，默认 4")
    ap.add_argument("--probe-only", action="store_true", help="只探测设备是否监听 OTA 帧")
    ap.add_argument("--verbose", action="store_true", help="逐帧打印耗时/响应类型（诊断用）")
    ap.add_argument("--rawlog", default=None, help="设备→PC 原始字节记录文件（hex，诊断用）")
    ap.add_argument("--monitor", action="store_true", help="探测/升级后进入日志监视模式（Ctrl+C 退出）")
    a = ap.parse_args()

    pkg_path = a.pkg if os.path.isabs(a.pkg) else os.path.join(HERE, a.pkg)
    if not os.path.exists(pkg_path):
        sys.exit(f"OTA 包不存在: {pkg_path}")

    print(f"[探测] {a.port}@{a.baud}（{a.attempts} 次 x {a.timeout}s）")
    resp = probe(a.port, a.baud, a.attempts, a.timeout, a.verbose)

    if resp is None:
        print(f"[探测] {a.attempts} 次均无响应 —— 设备 OTA 帧通道未监听")
        diagnose(a.port, a.baud)
        if a.monitor:
            monitor(a.port, a.baud)
        return 3

    ftype = {TYPE_ACK: "ACK", TYPE_RESUME: "RESUME"}.get(resp[0], hex(resp[0]))
    off = struct.unpack("<I", resp[2])[0] if len(resp[2]) >= 4 else 0
    print(f"[探测] 设备在听 OTA 帧：{ftype}，当前进度 {off} 字节")
    if off > 0:
        print("  [提示] 设备有残留进度，将从该偏移续传。")
        print("         若升级包与上次不同（版本/内容变化），续传会因包头不匹配卡住：")
        print("         请重烧最新固件（启动时会按版本自动清残留）或确认是同一包续传。")

    if a.probe_only:
        print("[探测] 完成（--probe-only）")
        if a.monitor:
            monitor(a.port, a.baud)
        return 0

    pkg = open(pkg_path, "rb").read()
    if pkg[0:4] != b"OTA1":
        sys.exit("不是有效 OTA 包（缺 OTA1 魔数）")
    print(f"[升级] {pkg_path}（{len(pkg)}B，载荷 {len(pkg)-HDR_LEN}B）")

    s = OtaSender(a.port, a.baud, pkg)
    if a.rawlog:
        s.rawlog = open(a.rawlog, "w", encoding="ascii")
    last = -1

    def cb(off_, total):
        nonlocal last
        pct = off_ * 100 // total
        if pct != last:
            last = pct
            print(f"  {pct:3d}%  {off_}/{total}")

    try:
        s.run(progress_cb=cb, verbose=a.verbose)
        print("[升级] 发送完成，设备正在验签（CRC32/HMAC/SHA256）并重启...")
        print("       复位后 bootloader commit(另一 bank, 逻辑0x100000段) -> swap(地址重映射) -> 新固件自检 -> confirm")
    except KeyboardInterrupt:
        print()
        print("已中断（Ctrl+C）。设备进度已保留，重跑本命令可续传。")
        if a.monitor:
            monitor_ser(s.ser, a.port, a.baud)
        if s.rawlog:
            s.rawlog.close()
        s.close()
        return 0

    if a.monitor:
        # 复用同一串口会话，避免重开丢缓冲（boot 打印在升级完成后发出）
        monitor_ser(s.ser, a.port, a.baud)
    if s.rawlog:
        s.rawlog.close()
    s.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
