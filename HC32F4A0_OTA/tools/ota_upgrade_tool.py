#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_upgrade_tool.py — 上位机 OTA 升级工具（Phase 6 第一步）

整合：版本库管理 + 网络升级（check/download，HTTP+Range 断点续传）+ 串口升级（OTA1 帧）

用法：
  版本库管理（本地 server_data/）
    python ota_upgrade_tool.py list --dir server_data
    python ota_upgrade_tool.py add fw.otapkg --platform 2 --version 0x01020000 --app 0 --dir server_data
    python ota_upgrade_tool.py remove --platform 2 --version 0x01020000 --dir server_data
  网络升级（连 ota_server）
    python ota_upgrade_tool.py check --server http://127.0.0.1:8080 --platform 2 --app 0 --version 0x01010000
    python ota_upgrade_tool.py download --server http://127.0.0.1:8080 --platform 2 --app 0 --version 0x01020000 --out fw_new.otapkg
  串口升级（本地通道）
    python ota_upgrade_tool.py uart --port COM3 --baud 115200 --pkg fw_new.otapkg
"""
import argparse, hashlib, hmac, json, os, struct, sys, urllib.request, zlib

HDR_LEN = 82
SIGN_OFF = 50
DEMO_KEY = b"HC32F4A0_OTA_KEY_DEMO_20260814"

def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def load_man(dirp):
    p = os.path.join(dirp, "manifest.json")
    if not os.path.exists(p):
        return {"platforms": {}}
    with open(p, "r", encoding="utf-8") as f:
        return json.load(f)

def save_man(dirp, man):
    with open(os.path.join(dirp, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(man, f, ensure_ascii=False, indent=2)

def sha256f(fp):
    h = hashlib.sha256()
    with open(fp, "rb") as f:
        for c in iter(lambda: f.read(65536), b""):
            h.update(c)
    return h.hexdigest()

def verify_pkg(fp):
    """校验 OTA 包：magic/crc32/sha256/hmac"""
    with open(fp, "rb") as f:
        pkg = f.read()
    hdr, payload = pkg[:HDR_LEN], pkg[HDR_LEN:]
    if hdr[:4] != b"OTA1":
        return "magic 错误"
    if struct.unpack("<I", hdr[10:14])[0] != len(payload):
        return "长度不匹配"
    if (zlib.crc32(payload) & 0xFFFFFFFF) != struct.unpack("<I", hdr[14:18])[0]:
        return "CRC32 错误"
    if hashlib.sha256(payload).digest() != hdr[18:50]:
        return "SHA256 错误"
    if hmac.new(DEMO_KEY, hdr[:SIGN_OFF] + payload, hashlib.sha256).digest() != hdr[50:82]:
        return "HMAC 错误"
    return None

# ---------------- 版本库管理 ----------------
def cmd_list(a):
    man = load_man(a.dir)
    for plat, m in man.get("platforms", {}).items():
        print(f"平台 {plat} (app_id={m.get('app_id',0)}, latest={m.get('latest','-')}):")
        for ver, info in m.get("versions", {}).items():
            print(f"  {ver}  {info.get('file')}  sha={info.get('sha256','')[:12]}")

def cmd_add(a):
    if verify_pkg(a.input) is not None:
        sys.exit("不是有效 OTA 包")
    man = load_man(a.dir)
    fwdir = os.path.join(a.dir, "firmware")
    os.makedirs(fwdir, exist_ok=True)
    dst = os.path.join(fwdir, os.path.basename(a.input))
    if os.path.abspath(dst) != os.path.abspath(a.input):
        with open(a.input, "rb") as s, open(dst, "wb") as d:
            d.write(s.read())
    plat = str(a.platform)
    man.setdefault("platforms", {})
    m = man["platforms"].setdefault(plat, {"app_id": a.app, "versions": {}})
    m["app_id"] = a.app
    ver = "0x%08X" % a.version
    m["versions"][ver] = {"file": os.path.basename(a.input), "sha256": sha256f(dst), "sign": "demo-hmac"}
    m["latest"] = ver
    save_man(a.dir, man)
    print(f"已添加 {ver} -> {os.path.basename(a.input)}（latest 更新）")

def cmd_remove(a):
    man = load_man(a.dir)
    plat = str(a.platform)
    m = man.get("platforms", {}).get(plat)
    if not m:
        sys.exit("平台不存在")
    ver = "0x%08X" % a.version
    info = m["versions"].pop(ver, None)
    if not info:
        sys.exit("版本不存在")
    fp = os.path.join(a.dir, "firmware", info["file"])
    if os.path.exists(fp):
        os.remove(fp)
    if m["latest"] == ver:
        m["latest"] = max(m["versions"].keys(), default=None)
    if not m["versions"]:
        man["platforms"].pop(plat)
    save_man(a.dir, man)
    print(f"已删除 {ver}")

# ---------------- 网络升级 ----------------
def api_check(server, platform, app, version):
    body = json.dumps({"device_id": "tool", "platform": platform, "app_id": app, "version": version}).encode()
    req = urllib.request.Request(server.rstrip("/") + "/ota/check", data=body, method="POST",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read().decode())

def cmd_check(a):
    r = api_check(a.server, a.platform, a.app, a.version)
    if r.get("upgrade"):
        print(f"有新版 {r['new_version']}  size={r['size']}  url={r['url']}")
        print(f"  sha256={r['sha256'][:16]}...")
    else:
        print("已是最新版本")

def cmd_download(a):
    r = api_check(a.server, a.platform, a.app, a.version)
    if not r.get("upgrade"):
        sys.exit("无可用升级")
    url = a.server.rstrip("/") + r["url"]
    out = a.out or os.path.basename(r["url"].split("id=")[-1])

    # HTTP+Range 断点续传（本地已有部分）
    offset = os.path.getsize(out) if os.path.exists(out) else 0
    headers = {"Range": f"bytes={offset}-"} if offset else {}
    if offset:
        print(f"本地已有 {offset}B，从断点续传...")
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as resp:
        mode = "ab" if offset else "wb"
        with open(out, mode) as f:
            while True:
                chunk = resp.read(65536)
                if not chunk:
                    break
                f.write(chunk)
    print(f"下载完成: {out} ({os.path.getsize(out)}B)")

    # 校验
    err = verify_pkg(out)
    if err:
        sys.exit(f"下载包校验失败: {err}")
    exp = r.get("sha256")
    if exp and sha256f(out) != exp:
        sys.exit("SHA256 与服务器不一致")
    print("校验通过（CRC32/SHA256/HMAC）")

# ---------------- 批量设备管理 ----------------
def cmd_devices(a):
    """批量 check（可选下载）：devices.csv = device_id,platform,app_id,version"""
    import csv as _csv
    if not os.path.exists(a.csv):
        sys.exit(f"设备清单不存在: {a.csv}")
    rows = []
    with open(a.csv, newline="", encoding="utf-8") as f:
        for r in _csv.DictReader(f):
            rows.append({
                "device_id": r.get("device_id", "").strip(),
                "platform": int(r.get("platform", "2")),
                "app_id": int(r.get("app_id", "0")),
                "version": int(r.get("version", "0"), 0) if r.get("version") else 0,
            })
    print(f"设备数: {len(rows)}")
    lines = []
    upg = 0
    for r in rows:
        resp = api_check(a.server, r["platform"], r["app_id"], r["version"])
        ok = resp.get("upgrade")
        ver = resp.get("new_version", "-")
        status = "需升级→" + ver if ok else "最新"
        print(f"  {r['device_id']:<16} v{hex(r['version'])}  {status}")
        lines.append(f"{r['device_id']},{r['platform']},{r['app_id']},{hex(r['version'])},{1 if ok else 0},{ver}")
        if ok:
            upg += 1
            if a.download:
                out = os.path.join(a.out_dir, f"{r['device_id']}.otapkg")
                os.makedirs(a.out_dir, exist_ok=True)
                url = a.server.rstrip("/") + resp["url"]
                req = urllib.request.Request(url)
                with urllib.request.urlopen(req, timeout=60) as rr, open(out, "wb") as f:
                    while True:
                        c = rr.read(65536)
                        if not c: break
                        f.write(c)
                err = verify_pkg(out)
                print(f"    -> 下载 {out} {'校验通过' if not err else '校验失败:'+err}")
    rep = a.out_dir and os.path.join(a.out_dir, "devices_report.csv") or "devices_report.csv"
    os.makedirs(os.path.dirname(rep) or ".", exist_ok=True)
    with open(rep, "w", encoding="utf-8") as f:
        f.write("device_id,platform,app_id,version,need_upgrade,new_version\n" + "\n".join(lines) + "\n")
    print(f"汇总: {upg}/{len(rows)} 台需升级 -> {rep}")

# ---------------- 串口升级 ----------------
def cmd_uart(a):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from ota_send import OtaSender, HDR_LEN as _H
    if verify_pkg(a.pkg) is not None:
        sys.exit("不是有效 OTA 包")
    pkg = open(a.pkg, "rb").read()
    s = OtaSender(a.port, a.baud, pkg)
    last = -1
    def cb(off, total):
        nonlocal last
        pct = off * 100 // total
        if pct != last:
            last = pct
            print(f"  {pct:3d}%  {off}/{total}")
    print(f"串口升级 {a.pkg} @ {a.port}@{a.baud}（{len(pkg)}B）")
    try:
        s.run(progress_cb=cb)
        print("发送完成，设备正在验签并重启...")
    except KeyboardInterrupt:
        print("\n已中断（Ctrl+C），进度已保留，重跑可续传")
    finally:
        s.close()

def main():
    ep()
    ap = argparse.ArgumentParser(description="上位机 OTA 升级工具")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("list", help="列出版本库"); p.add_argument("--dir", default="server_data"); p.set_defaults(fn=cmd_list)
    p = sub.add_parser("add", help="添加固件到版本库"); p.add_argument("input"); p.add_argument("--platform", type=int, default=2)
    p.add_argument("--version", type=lambda x: int(x, 0), default=0x01020000); p.add_argument("--app", type=int, default=0)
    p.add_argument("--dir", default="server_data"); p.set_defaults(fn=cmd_add)
    p = sub.add_parser("remove", help="从版本库删除"); p.add_argument("--platform", type=int, default=2)
    p.add_argument("--version", type=lambda x: int(x, 0)); p.add_argument("--dir", default="server_data"); p.set_defaults(fn=cmd_remove)
    p = sub.add_parser("check", help="查询升级"); p.add_argument("--server", required=True)
    p.add_argument("--platform", type=int, default=2); p.add_argument("--app", type=int, default=0)
    p.add_argument("--version", type=lambda x: int(x, 0), default=0); p.set_defaults(fn=cmd_check)
    p = sub.add_parser("download", help="下载并校验固件"); p.add_argument("--server", required=True)
    p.add_argument("--platform", type=int, default=2); p.add_argument("--app", type=int, default=0)
    p.add_argument("--version", type=lambda x: int(x, 0), default=0); p.add_argument("--out", default=None); p.set_defaults(fn=cmd_download)
    p = sub.add_parser("uart", help="串口升级"); p.add_argument("--port", required=True)
    p.add_argument("--baud", type=int, default=115200); p.add_argument("--pkg", required=True); p.set_defaults(fn=cmd_uart)
    p = sub.add_parser("devices", help="批量设备 check/下载"); p.add_argument("--server", required=True)
    p.add_argument("--csv", required=True, help="设备清单 CSV: device_id,platform,app_id,version")
    p.add_argument("--download", action="store_true"); p.add_argument("--out-dir", default="devices_out")
    p.set_defaults(fn=cmd_devices)

    a = ap.parse_args()
    a.fn(a)

if __name__ == "__main__":
    main()
