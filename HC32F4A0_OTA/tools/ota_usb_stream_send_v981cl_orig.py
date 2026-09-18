#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""USB stream OTA sender (v9.75): 82B header + 4KB frames + probe/retransmit loop.
   Device EFM-writes other bank directly; lost frames are recovered via
           periodic len=0 probe (device ACKs current offset, sender refills gap)."""
import argparse, struct, sys, time, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ota_send import make_frame, parse_frame, TYPE_DATA, TYPE_ACK, TYPE_RESUME

def read_ack(ser, timeout):
    end = time.time() + timeout
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
        pl = struct.unpack("<H", head[7:9])[0]
        tail = ser.read(pl + 2)
        if len(tail) < pl + 2:
            return None
        return parse_frame(head + tail)
    return None

def main():
    ap = argparse.ArgumentParser(description="USB stream OTA sender")
    ap.add_argument("pkg", help=".otapkg file")
    ap.add_argument("--port", required=True, help="CDC port, e.g. COM8")
    ap.add_argument("--baud", type=int, default=115200)
    a = ap.parse_args()

    pkg = open(a.pkg, "rb").read()
    if pkg[0:4] != b"OTA1":
        sys.exit("bad package (no OTA1 magic)")
    hdr = pkg[:82]
    payload = pkg[82:]
    total = len(payload)

    import serial
    ser = serial.Serial(a.port, a.baud, timeout=1.0, write_timeout=3.0,
                        dsrdtr=False, rtscts=False)
    ser.setDTR(False)
    ser.setRTS(False)
    ser.reset_input_buffer()
    t0 = time.time()

    # 1) header frame
    ser.write(make_frame(TYPE_DATA, 0, hdr))
    ser.flush()

    # 2) wait ACK(0): burst 5 header frames (raise arrival odds on flaky USB),
    #    then listen. v9.78 restarts session on any header frame (harmless dup).
    ack = None
    for attempt in range(6):
        ser.reset_input_buffer()
        for _ in range(5):
            ser.write(make_frame(TYPE_DATA, 0, hdr))
        ser.flush()
        ack = read_ack(ser, 25.0 if attempt == 0 else 15.0)
        if ack:
            break
        print("[retry] header burst #%d..." % (attempt + 2))
    if not ack:
        print("!! no ACK after header (device prepare failed?)")
        ser.close()
        return 2
    print("[prep] device ACK, streaming %d bytes..." % total)

    # 3) stream 4KB frames, paced by DEVICE progress (v9.81g)
    #    Device QSPI erase+write ~50ms/frame (~80KB/s); sender must NOT
    #    run ahead. Send small batch, wait for device ACK, flush stale
    #    ACKs, rewind to confirmed offset. Progress = DEVICE offset.
    off = 0
    t1 = time.time()
    last_pct = -1
    dev_off = 0
    def prog(off_, total_):
        nonlocal last_pct
        pct = off_ * 100 // total_
        if pct != last_pct:
            last_pct = pct
            el = time.time() - t1
            rate = off_ / el / 1024 if el > 0 else 0
            print("  %3d%%  %d/%d  (%.1f KB/s)" % (pct, off_, total_, rate))
    noack = 0
    while dev_off < len(payload):
        # v9.81cl: batch 2 frames to match device ACK cadence + 8KB RX buffer
        batch = 2
        for _ in range(batch):
            chunk = payload[dev_off:dev_off + 4096]
            if not chunk:
                break
            seq = dev_off // 4096 + 1
            ser.write(make_frame(TYPE_DATA, seq & 0xFFFF, chunk))
            dev_off += len(chunk)
        ser.flush()
        ser.reset_input_buffer()
        ack = read_ack(ser, 3.0)
        if ack and ack[0] in (TYPE_ACK, TYPE_RESUME) and len(ack[2]) >= 4:
            d = struct.unpack("<I", ack[2])[0]
            if d <= dev_off:
                dev_off = d
            noack = 0
        else:
            noack += 1
            if noack >= 10:
                print("[send] no ACK, device stalled at %d" % dev_off)
                break
        prog(dev_off, len(payload))
    dt = time.time() - t1
    print("[send] device at %d/%d in %.2fs = %.0f KB/s" % (dev_off, len(payload), dt, dev_off / dt / 1024 if dt > 0 else 0))

    # 4) probe + retransmit gaps until device reports offset >= total
    noresp = 0
    round_ = 0
    while round_ < 300:
        round_ += 1
        ser.reset_input_buffer()
        ser.write(make_frame(TYPE_DATA, 0xFFFF, b""))   # len=0 probe
        ack = read_ack(ser, 2.5)
        if ack and ack[0] in (TYPE_ACK, TYPE_RESUME) and len(ack[2]) >= 4:
            dev_off = struct.unpack("<I", ack[2])[0]
            noresp = 0
            if dev_off >= total:
                print("[verify] device offset %d >= %d, verifying+swap+reset..." % (dev_off, total))
                time.sleep(3)
                break
            prog(dev_off, len(payload))
            if dev_off < len(payload):
                chunk = payload[dev_off:dev_off + 4096]
                if chunk:
                    ser.write(make_frame(TYPE_DATA, (dev_off // 4096 + 1) & 0xFFFF, chunk))
        else:
            noresp += 1
            if noresp >= 25:
                print("[done] device gone (reset after swap)")
                break
            time.sleep(1)

    ser.close()
    el = time.time() - t0
    print("[done] total %.1fs (send %.1fs, %d refill rounds)" % (el, dt, round_))
    return 0

if __name__ == "__main__":
    sys.exit(main())
