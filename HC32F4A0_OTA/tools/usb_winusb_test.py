#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
WinUSB 测试/收数工具（纯 ctypes，无需安装 pyusb/libusb）

用途：F460 扫描盘在 usb_type=3/4（含 WinUSB 组合）时，PC 侧验证
  1) Windows 是否已把设备绑定到 WinUSB（微软 OS 描述符是否生效）
  2) 从 BULK IN 端点读上传帧，并解析出 EPC

用法：
  python usb_winusb_test.py --list              # 列出设备与端点（先看这个）
  python usb_winusb_test.py --read              # 持续收帧并解析
  python usb_winusb_test.py --read --raw        # 同时打印原始 hex
  python usb_winusb_test.py --read --seconds 30 # 只收 30 秒

上传帧格式（与网口 TCP 上传同构）：
  0xFF | nameLen | dataLen(2B BE) | msgType | flags | errCode(4B) | readerName |
  tagCnt(2B BE) | tag...
tag 记录： antenna | readCnt | rssi(int8) | protocol | epclen | epc[epclen] |
           embLen | embData[embLen]
"""
import argparse
import ctypes
import sys
import time
from ctypes import wintypes

GUID_DEVINTERFACE_USB_DEVICE = "{A5DCBF10-6530-11D2-901F-00C04FB951ED}"
DIGCF_PRESENT = 0x02
DIGCF_DEVICEINTERFACE = 0x10
GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

PIPE_TRANSFER_TIMEOUT = 0x03
RAW_IO = 0x07

MSG_TYPES = {
    0: "None", 1: "TagRead", 2: "GpiTrigger", 3: "TagComing", 4: "HeartBeat",
    5: "RdrError", 6: "SyncTimeReq", 20: "GetConf", 21: "SetConf",
    34: "GetCurWorkMode", 35: "SwitchWorkMode", 36: "GetRunTimeConf",
    37: "SetRunTimeConf", 100: "TestUart1ex",
}


class GUID(ctypes.Structure):
    _fields_ = [("Data1", wintypes.DWORD), ("Data2", wintypes.WORD),
                ("Data3", wintypes.WORD), ("Data4", ctypes.c_ubyte * 8)]


class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [("cbSize", wintypes.DWORD), ("InterfaceClassGuid", GUID),
                ("Flags", wintypes.DWORD), ("Reserved", ctypes.POINTER(ctypes.c_ulong))]


class SP_DEVICE_INTERFACE_DETAIL_DATA_W(ctypes.Structure):
    _fields_ = [("cbSize", wintypes.DWORD), ("DevicePath", ctypes.c_wchar * 1)]


class USB_INTERFACE_DESCRIPTOR(ctypes.Structure):
    _fields_ = [("bLength", ctypes.c_ubyte), ("bDescriptorType", ctypes.c_ubyte),
                ("bInterfaceNumber", ctypes.c_ubyte), ("bAlternateSetting", ctypes.c_ubyte),
                ("bNumEndpoints", ctypes.c_ubyte), ("bInterfaceClass", ctypes.c_ubyte),
                ("bInterfaceSubClass", ctypes.c_ubyte), ("bInterfaceProtocol", ctypes.c_ubyte),
                ("iInterface", ctypes.c_ubyte)]


class WINUSB_PIPE_INFORMATION(ctypes.Structure):
    _fields_ = [("PipeType", ctypes.c_uint), ("PipeId", ctypes.c_ubyte),
                ("MaximumPacketSize", ctypes.c_ushort), ("Interval", ctypes.c_ubyte)]


def guid_from_string(s):
    g = GUID()
    s = s.strip("{}").replace("-", "")
    if len(s) != 32:
        raise ValueError("bad GUID: %s" % s)
    p = [int(s[i:i + 2], 16) for i in range(0, 32, 2)]
    g.Data1 = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]
    g.Data2 = (p[4] << 8) | p[5]
    g.Data3 = (p[6] << 8) | p[7]
    for i in range(8):
        g.Data4[i] = p[8 + i]
    return g


setupapi = ctypes.WinDLL("setupapi", use_last_error=True)
winusb = ctypes.WinDLL("winusb", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

# Without explicit prototypes ctypes truncates 64-bit HANDLEs to int and every
# call fails silently - declare everything we use.
setupapi.SetupDiGetClassDevsW.restype = wintypes.HANDLE
setupapi.SetupDiGetClassDevsW.argtypes = [ctypes.POINTER(GUID), wintypes.LPCWSTR,
                                         wintypes.HWND, wintypes.DWORD]
setupapi.SetupDiEnumDeviceInterfaces.restype = wintypes.BOOL
setupapi.SetupDiEnumDeviceInterfaces.argtypes = [wintypes.HANDLE, ctypes.c_void_p,
                                                 ctypes.POINTER(GUID), wintypes.DWORD,
                                                 ctypes.POINTER(SP_DEVICE_INTERFACE_DATA)]
setupapi.SetupDiGetDeviceInterfaceDetailW.restype = wintypes.BOOL
setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [wintypes.HANDLE,
                                                      ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),
                                                      ctypes.c_void_p, wintypes.DWORD,
                                                      ctypes.POINTER(wintypes.DWORD),
                                                      ctypes.c_void_p]
setupapi.SetupDiDestroyDeviceInfoList.restype = wintypes.BOOL
setupapi.SetupDiDestroyDeviceInfoList.argtypes = [wintypes.HANDLE]

kernel32.CreateFileW.restype = wintypes.HANDLE
kernel32.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                 ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
                                 wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]

winusb.WinUsb_Initialize.restype = wintypes.BOOL
winusb.WinUsb_Initialize.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_void_p)]
winusb.WinUsb_Free.restype = wintypes.BOOL
winusb.WinUsb_Free.argtypes = [ctypes.c_void_p]
winusb.WinUsb_QueryInterfaceSettings.restype = wintypes.BOOL
winusb.WinUsb_QueryInterfaceSettings.argtypes = [ctypes.c_void_p, ctypes.c_ubyte,
                                                 ctypes.POINTER(USB_INTERFACE_DESCRIPTOR)]
winusb.WinUsb_QueryPipe.restype = wintypes.BOOL
winusb.WinUsb_QueryPipe.argtypes = [ctypes.c_void_p, ctypes.c_ubyte, ctypes.c_ubyte,
                                    ctypes.POINTER(WINUSB_PIPE_INFORMATION)]
winusb.WinUsb_SetPipePolicy.restype = wintypes.BOOL
winusb.WinUsb_SetPipePolicy.argtypes = [ctypes.c_void_p, ctypes.c_ubyte, wintypes.ULONG,
                                        wintypes.ULONG, ctypes.c_void_p]
winusb.WinUsb_ReadPipe.restype = wintypes.BOOL
winusb.WinUsb_ReadPipe.argtypes = [ctypes.c_void_p, ctypes.c_ubyte, ctypes.c_void_p,
                                   wintypes.ULONG, ctypes.POINTER(wintypes.ULONG),
                                   ctypes.c_void_p]


def enum_usb_devices(vid=None, pid=None):
    """yield (device_path, description)"""
    guid = guid_from_string(GUID_DEVINTERFACE_USB_DEVICE)
    hdev = setupapi.SetupDiGetClassDevsW(ctypes.byref(guid), None, None,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    if hdev == INVALID_HANDLE_VALUE:
        return
    try:
        ifdata = SP_DEVICE_INTERFACE_DATA()
        ifdata.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)
        i = 0
        while setupapi.SetupDiEnumDeviceInterfaces(hdev, None, ctypes.byref(guid), i,
                                                  ctypes.byref(ifdata)):
            i += 1
            need = wintypes.DWORD(0)
            setupapi.SetupDiGetDeviceInterfaceDetailW(hdev, ctypes.byref(ifdata), None, 0,
                                                      ctypes.byref(need), None)
            if need.value == 0:
                continue
            buf = ctypes.create_string_buffer(need.value)
            detail = ctypes.cast(buf, ctypes.POINTER(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
            detail.contents.cbSize = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6
            if not setupapi.SetupDiGetDeviceInterfaceDetailW(hdev, ctypes.byref(ifdata),
                                                             detail, need.value,
                                                             ctypes.byref(need), None):
                continue
            path = ctypes.wstring_at(ctypes.addressof(buf) + 4)
            low = path.lower()
            if vid and ("vid_%04x" % vid) not in low:
                continue
            if pid and ("pid_%04x" % pid) not in low:
                continue
            yield path
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(hdev)


def open_winusb(path):
    h = kernel32.CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, None,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, None)
    if h == INVALID_HANDLE_VALUE:
        raise OSError("CreateFile failed (err=%d)" % ctypes.get_last_error())
    wu = ctypes.c_void_p()
    if not winusb.WinUsb_Initialize(h, ctypes.byref(wu)):
        kernel32.CloseHandle(h)
        raise OSError("WinUsb_Initialize failed: device is not bound to WinUSB "
                      "(check Device Manager / MS OS descriptor)")
    return h, wu


def query_pipes(wu):
    desc = USB_INTERFACE_DESCRIPTOR()
    if not winusb.WinUsb_QueryInterfaceSettings(wu, 0, ctypes.byref(desc)):
        return desc, []
    pipes = []
    for i in range(desc.bNumEndpoints):
        pi = WINUSB_PIPE_INFORMATION()
        if winusb.WinUsb_QueryPipe(wu, 0, i, ctypes.byref(pi)):
            pipes.append(pi)
    return desc, pipes


def cmd_list(args):
    found = False
    for path in enum_usb_devices(args.vid, args.pid):
        found = True
        print("device: %s" % path)
        try:
            h, wu = open_winusb(path)
        except OSError as e:
            print("  !! %s" % e)
            continue
        try:
            desc, pipes = query_pipes(wu)
            print("  interface: class=0x%02X sub=0x%02X proto=0x%02X endpoints=%d" %
                  (desc.bInterfaceClass, desc.bInterfaceSubClass, desc.bInterfaceProtocol,
                   desc.bNumEndpoints))
            for p in pipes:
                kind = {0: "control", 1: "isochronous", 2: "bulk", 3: "interrupt"}.get(
                    p.PipeType, str(p.PipeType))
                print("    pipe 0x%02X  %-11s mps=%d interval=%d" %
                      (p.PipeId, kind, p.MaximumPacketSize, p.Interval))
        finally:
            winusb.WinUsb_Free(wu)
            kernel32.CloseHandle(h)
    if not found:
        print("no USB device matched VID=%04X PID=%04X" % (args.vid, args.pid))
        print("hint: check Device Manager; if it shows an unknown device, the MS OS")
        print("      descriptors did not take effect (unplug/replug, or bind winusb.sys)")
    return 0


def parse_frame(b):
    if len(b) < 10 or b[0] != 0xFF:
        return None
    namelen = b[1]
    datalen = (b[2] << 8) | b[3]
    mtype = b[4]
    flags = b[5]
    errcode = int.from_bytes(b[6:10], "big")
    if len(b) < 10 + namelen:
        return None
    name = b[10:10 + namelen].decode("latin-1", "replace")
    body = b[10 + namelen:10 + namelen + datalen]
    tags = []
    if mtype == 1 and len(body) >= 2:
        cnt = (body[0] << 8) | body[1]
        pos = 2
        for _ in range(cnt):
            if pos + 5 > len(body):
                break
            antenna, readcnt, rssi, proto, epclen = body[pos:pos + 5]
            pos += 5
            if pos + epclen > len(body):
                break
            epc = body[pos:pos + epclen].hex().upper()
            pos += epclen
            if pos >= len(body):
                break
            emblen = body[pos]
            pos += 1 + emblen
            tags.append((antenna, readcnt, rssi - 256 if rssi > 127 else rssi, proto, epc))
    return dict(name=name, mtype=mtype, flags=flags, err=errcode, tags=tags, body=body)


def cmd_read(args):
    paths = list(enum_usb_devices(args.vid, args.pid))
    if not paths:
        print("no USB device matched VID=%04X PID=%04X" % (args.vid, args.pid))
        return 1
    h, wu = open_winusb(paths[0])
    desc, pipes = query_pipes(wu)
    print("opened %s" % paths[0])
    print("interface class=0x%02X endpoints=%d" % (desc.bInterfaceClass, desc.bNumEndpoints))
    for p in pipes:
        if p.PipeType == 2 and (p.PipeId & 0x80):
            print("bulk IN pipe 0x%02X mps=%d" % (p.PipeId, p.MaximumPacketSize))
    timeout = ctypes.c_ulong(args.timeout)
    winusb.WinUsb_SetPipePolicy(wu, args.pipe, PIPE_TRANSFER_TIMEOUT,
                                ctypes.sizeof(timeout), ctypes.byref(timeout))
    buf = ctypes.create_string_buffer(args.buflen)
    got = ctypes.c_ulong(0)
    t0 = time.time()
    try:
        while True:
            if args.seconds and (time.time() - t0) > args.seconds:
                break
            ok = winusb.WinUsb_ReadPipe(wu, args.pipe, buf, args.buflen,
                                       ctypes.byref(got), None)
            if not ok:
                print("read failed (err=%d) - unplugged?" % ctypes.get_last_error())
                break
            if got.value == 0:
                continue
            data = buf.raw[:got.value]
            ts = time.strftime("%H:%M:%S")
            if args.raw or not data[:1] == b"\xff":
                print("[%s] raw(%d): %s" % (ts, len(data), data.hex().upper()))
            f = parse_frame(data)
            if f is None:
                continue
            print("[%s] %-12s name=%s flags=0x%02X err=%d len=%d" %
                  (ts, MSG_TYPES.get(f["mtype"], str(f["mtype"])), f["name"],
                   f["flags"], f["err"], len(f["body"])))
            for ant, rcnt, rssi, proto, epc in f["tags"]:
                print("      ant=%d cnt=%d rssi=%d proto=%d epc=%s" %
                      (ant, rcnt, rssi, proto, epc))
    except KeyboardInterrupt:
        pass
    finally:
        winusb.WinUsb_Free(wu)
        kernel32.CloseHandle(h)
    return 0


def main():
    ap = argparse.ArgumentParser(description="F460 WinUSB test tool (no extra packages)")
    ap.add_argument("--list", action="store_true", help="list device + endpoints")
    ap.add_argument("--read", action="store_true", help="read bulk IN and parse frames")
    ap.add_argument("--vid", type=lambda x: int(x, 16), default=0x2E88)
    # WinUSB 组合用 0x4608（与 F4A0 / 上位机 WinUSB 通道一致）；
    # 若设备是 HID/CDC/键盘组合（PID 0x4605）也要看端点，可显式传 --pid 4605
    ap.add_argument("--pid", type=lambda x: int(x, 16), default=0x4608)
    ap.add_argument("--pipe", type=lambda x: int(x, 16), default=0x82)
    ap.add_argument("--seconds", type=int, default=0, help="0 = forever")
    ap.add_argument("--timeout", type=int, default=2000, help="read timeout ms")
    ap.add_argument("--buflen", type=int, default=4096)
    ap.add_argument("--raw", action="store_true", help="always hex dump")
    args = ap.parse_args()
    if args.list:
        return cmd_list(args)
    if args.read:
        return cmd_read(args)
    ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
