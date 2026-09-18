#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""HTTP OTA sender (v9.81j, progress v9.81bu):
PC connects to device HTTP server and POSTs the .otapkg in chunks with progress.

Device listens on listenPort+1 (independent channel, no serial/USB coupling).
Device http_handle_conn reads body by Content-Length and writes staging
streamingly (ota_storage_write_stage), so chunked upload works and the
client can show send progress.

Usage:
  python ota_http_send.py fw_acc.otapkg --host <device-ip> [--port 8081]
"""
import argparse, sys, time, socket

CHUNK = 16 * 1024  # 16KB per write


def main():
    ap = argparse.ArgumentParser(description="HTTP OTA sender (device HTTP server)")
    ap.add_argument("pkg", help=".otapkg file")
    ap.add_argument("--host", required=True, help="device IP")
    ap.add_argument("--port", type=int, default=8081, help="device HTTP OTA port (listenPort+1)")
    a = ap.parse_args()

    with open(a.pkg, "rb") as f:
        data = f.read()
    if data[0:4] != b"OTA1":
        sys.exit("bad package (no OTA1 magic)")
    total = len(data)
    print("[http] POST %d bytes to %s:%d/ota (chunked %dKB) ..."
          % (total, a.host, a.port, CHUNK // 1024))

    t0 = time.time()
    sent = 0
    last_t = t0
    try:
        sock = socket.create_connection((a.host, a.port), timeout=10)
        # send timeout: cable pull -> sendall blocks, catch with timeout
        sock.settimeout(10)
        req = ("POST /ota HTTP/1.1\r\n"
               "Host: %s:%d\r\n"
               "Content-Type: application/octet-stream\r\n"
               "Content-Length: %d\r\n"
               "Connection: close\r\n\r\n" % (a.host, a.port, total))
        sock.sendall(req.encode())

        while sent < total:
            chunk = data[sent:sent + CHUNK]
            sock.sendall(chunk)
            sent += len(chunk)
            now = time.time()
            pct = sent * 100.0 / total
            rate = sent / 1024.0 / (now - t0)
            # 逐行显示（与串口/USB 风格一致）
            if (int(pct) % 5 == 0 and now - last_t >= 0.5) or sent == total:
                print("  %3d%%  %d/%d KB  (%.1f KB/s)"
                      % (pct, sent // 1024, total // 1024, rate))
                sys.stdout.flush()
                last_t = now

        # read device response
        sock.settimeout(15)
        resp = b""
        while True:
            try:
                b = sock.recv(4096)
            except socket.timeout:
                break
            if not b:
                break
            resp += b
        sock.close()
        dt = time.time() - t0
        print("[http] sent %d bytes in %.2fs = %.0f KB/s" % (total, dt, total / dt / 1024))
        body = resp.split(b"\r\n\r\n", 1)[-1] if b"\r\n\r\n" in resp else resp
        print("[http] device resp: %s" % body.decode("utf-8", "replace"))
        print("[http] device verifying+swap+reset... (watch COM3 monitor)")
    except socket.timeout:
        print("[http] ERROR: timed out - network lost or device busy (cable pulled?)")
        return 1
    except Exception as e:
        print("[http] ERROR: %s" % e)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
