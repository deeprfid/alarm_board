#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rk_ota_agent.py — RK3506G/RK3566 应用升级 Agent（Phase 2，只升应用不升系统）

流程（对齐统一 OTA 语义）：
  check → download(staging, 断点续传) → verify(CRC32/SHA256/HMAC)
  → 备份旧应用（原子）→ rename 原子替换 → 服务重启（systemd）→ 记录版本
  → 失败回滚（从备份恢复旧应用）

应用升级用"rename 原子替换"（借鉴 Mender Update Modules 语义）：
  写 staging/app.new → os.replace(staging/app.new, app_path) —— 单次 rename 原子提交，
  任何时刻磁盘上都有完整可用的旧应用或新应用，杜绝半写状态。

配置示例 rk_agent.json：
{
  "server": "http://127.0.0.1:8080",
  "device_id": "rk3506g-001",
  "platform": 3,
  "app_id": 0,
  "app_path": "/opt/myapp/myapp",
  "service": "myapp.service",
  "staging_dir": "/ota/staging",
  "backup_dir": "/ota/backup",
  "key": "HC32F4A0_OTA_KEY_DEMO_20260814",
  "state_file": "/var/lib/ota/rk_state.json",
  "restart_service": true
}

用法：
  python rk_ota_agent.py --config rk_agent.json          # 执行一次升级检查+升级
  python rk_ota_agent.py --selftest                      # 文件系统级自测（临时目录模拟）
"""
import argparse, hashlib, hmac, json, os, shutil, struct, sys, urllib.request, zlib

HDR_LEN, SIGN_OFF = 82, 50

def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def sha256f(fp):
    h = hashlib.sha256()
    with open(fp, "rb") as f:
        for c in iter(lambda: f.read(65536), b""):
            h.update(c)
    return h.hexdigest()

def verify_pkg(fp, key):
    """0=通过；否则返回错误串"""
    with open(fp, "rb") as f:
        pkg = f.read()
    hdr, payload = pkg[:HDR_LEN], pkg[HDR_LEN:]
    if hdr[:4] != b"OTA1": return "magic"
    if struct.unpack("<I", hdr[10:14])[0] != len(payload): return "len"
    if (zlib.crc32(payload) & 0xFFFFFFFF) != struct.unpack("<I", hdr[14:18])[0]: return "crc32"
    if hashlib.sha256(payload).digest() != hdr[18:50]: return "sha256"
    if hmac.new(key.encode(), hdr[:SIGN_OFF] + payload, hashlib.sha256).digest() != hdr[50:82]: return "hmac"
    return None

def check_upgrade(cfg, cur_ver):
    body = json.dumps({"device_id": cfg["device_id"], "platform": cfg["platform"],
                       "app_id": cfg["app_id"], "version": cur_ver}).encode()
    req = urllib.request.Request(cfg["server"].rstrip("/") + "/ota/check", data=body, method="POST",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode())

def download_pkg(cfg, url, dest, size):
    """HTTP+Range 断点续传下载"""
    offset = os.path.getsize(dest) if os.path.exists(dest) else 0
    if offset > 0:
        print(f"  断点续传 from {offset}B")
    req = urllib.request.Request(cfg["server"].rstrip("/") + url, headers={"Range": f"bytes={offset}-"} if offset else {})
    with urllib.request.urlopen(req, timeout=120) as r:
        mode = "ab" if offset else "wb"
        with open(dest, mode) as f:
            while True:
                c = r.read(65536)
                if not c: break
                f.write(c)
    return os.path.getsize(dest)

def atomic_replace(src, dst):
    """rename 原子替换：src → dst（同文件系统保证原子性）"""
    os.replace(src, dst)

def backup_app(cfg):
    """备份旧应用：先写临时再 rename，保证备份完整"""
    if not os.path.exists(cfg["app_path"]):
        return None
    os.makedirs(cfg["backup_dir"], exist_ok=True)
    bak = os.path.join(cfg["backup_dir"], "app.bak")
    tmp = bak + ".tmp"
    shutil.copy2(cfg["app_path"], tmp)
    atomic_replace(tmp, bak)
    return bak

def restart_service(cfg):
    if not cfg.get("restart_service", True):
        print("  (restart_service=false，跳过 systemctl)")
        return 0
    import subprocess
    r = subprocess.run(["systemctl", "restart", cfg["service"]], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"  systemctl restart 失败: {r.stderr.strip()}")
    return r.returncode

def read_state(cfg):
    sp = cfg["state_file"]
    if os.path.exists(sp):
        with open(sp) as f:
            return json.load(f).get("version", 0)
    return 0

def write_state(cfg, version):
    os.makedirs(os.path.dirname(cfg["state_file"]), exist_ok=True)
    with open(cfg["state_file"], "w") as f:
        json.dump({"version": version}, f, indent=2)

def run_once(cfg):
    cur_ver = read_state(cfg)
    print(f"当前版本: {hex(cur_ver) if cur_ver else 0}")
    resp = check_upgrade(cfg, cur_ver)
    if not resp.get("upgrade"):
        print("已是最新版本")
        return 0
    new_ver = int(resp["new_version"], 0)
    print(f"发现新版 {hex(new_ver)} size={resp['size']}")

    os.makedirs(cfg["staging_dir"], exist_ok=True)
    stage = os.path.join(cfg["staging_dir"], f"app_{hex(new_ver)}.otapkg")

    # 1) 下载（断点续传）+ 校验
    sz = download_pkg(cfg, resp["url"], stage, resp["size"])
    err = verify_pkg(stage, cfg["key"])
    if err:
        print(f"包校验失败: {err} → 中止（旧应用不动）")
        return -1
    print(f"  校验通过 size={sz}")

    # 2) 备份旧应用
    bak = backup_app(cfg)
    print(f"  备份: {bak}")

    try:
        # 3) rename 原子替换（staging 包内 payload = 应用）
        with open(stage, "rb") as f:
            f.seek(HDR_LEN)
            payload = f.read()
        os.makedirs(os.path.dirname(cfg["app_path"]), exist_ok=True)
        tmp = cfg["app_path"] + ".new"
        with open(tmp, "wb") as f:
            f.write(payload)
        os.chmod(tmp, 0o755)
        atomic_replace(tmp, cfg["app_path"])      # ← 原子提交点
        print(f"  原子替换: {cfg['app_path']}")

        # 4) 记录版本 + 重启服务
        write_state(cfg, new_ver)
        restart_service(cfg)
        print("升级完成")
        return 0
    except Exception as e:
        print(f"升级失败: {e} → 回滚")
        if bak and os.path.exists(bak):
            tmp = cfg["app_path"] + ".rollback"
            shutil.copy2(bak, tmp)
            atomic_replace(tmp, cfg["app_path"])
            print(f"  已从备份恢复 {cfg['app_path']}")
        return -1

def selftest():
    """文件系统级自测：临时目录模拟 app/staging/backup + 本地"服务器"包"""
    import tempfile, uuid
    # 兼容受限环境：tempfile.mkdtemp 目录在部分沙箱下不可写，改用 makedirs + uuid
    base = os.path.abspath(os.environ.get("OTA_ACC_TMP", "."))
    tmp = os.path.join(base, "rk_ota_test_" + uuid.uuid4().hex)
    os.makedirs(tmp, exist_ok=False)
    try:
        appdir = os.path.join(tmp, "opt", "myapp")
        os.makedirs(appdir)
        app_path = os.path.join(appdir, "myapp")
        with open(app_path, "w") as f:
            f.write("OLD-APP-V1")
        cfg = {
            "server": "http://127.0.0.1:9",   # 端口 9 必然连接失败 → 走离线自测路径
            "device_id": "rk-test", "platform": 3, "app_id": 0,
            "app_path": app_path,
            "service": "myapp.service",
            "staging_dir": os.path.join(tmp, "ota", "staging"),
            "backup_dir": os.path.join(tmp, "ota", "backup"),
            "key": "HC32F4A0_OTA_KEY_DEMO_20260814",
            "state_file": os.path.join(tmp, "var", "lib", "ota", "state.json"),
            "restart_service": False,
        }
        # 造一个 OTA 包（payload = 新应用内容）
        import ota_pack
        new_app = b"NEW-APP-V2-DATA" * 64
        pkg_path = os.path.join(tmp, "app_v2.otapkg")
        pkg = ota_pack.build_pkg(new_app, 0x01020000, 3, 0, cfg["key"].encode())
        with open(pkg_path, "wb") as f:
            f.write(pkg)

        # 直接验证 verify_pkg
        assert verify_pkg(pkg_path, cfg["key"]) is None, "包校验应通过"
        print("[PASS] OTA 包校验（CRC32/SHA256/HMAC）")

        # 模拟下载（本地复制到 staging）
        os.makedirs(cfg["staging_dir"], exist_ok=True)
        stage = os.path.join(cfg["staging_dir"], "app_0x01020000.otapkg")
        shutil.copy2(pkg_path, stage)

        # 执行替换流程（复用 run_once 的替换部分）
        bak = backup_app(cfg)
        assert os.path.exists(bak), "备份应存在"
        with open(stage, "rb") as f:
            f.seek(HDR_LEN); payload = f.read()
        tmpf = app_path + ".new"
        with open(tmpf, "wb") as f:
            f.write(payload)
        os.chmod(tmpf, 0o755)
        atomic_replace(tmpf, app_path)
        write_state(cfg, 0x01020000)
        with open(app_path, "rb") as f:
            got = f.read()
        assert got == new_app, "新应用内容应被原子替换"
        assert read_state(cfg) == 0x01020000, "版本应记录"
        print("[PASS] rename 原子替换 + 版本记录")

        # 回滚测试：破坏新应用后从备份恢复
        with open(app_path, "w") as f:
            f.write("CORRUPT")
        tmpf = app_path + ".rollback"
        shutil.copy2(bak, tmpf)
        atomic_replace(tmpf, app_path)
        with open(app_path, "r") as f:
            assert f.read() == "OLD-APP-V1", "应恢复旧应用"
        print("[PASS] 失败回滚（备份恢复）")

        # 校验失败中止测试：坏包不应触碰 app
        bad = bytearray(pkg)
        bad[14] ^= 0xFF                     # 破坏 CRC32
        bad_path = os.path.join(tmp, "bad.otapkg")
        with open(bad_path, "wb") as f:
            f.write(bytes(bad))
        assert verify_pkg(bad_path, cfg["key"]) is not None, "坏包校验应失败"
        print("[PASS] 坏包被拒（不触碰旧应用）")

        print("\n全部通过：RK Agent 原子替换/回滚/校验")
        return 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

def main():
    ep()
    ap = argparse.ArgumentParser(description="RK 应用升级 Agent")
    ap.add_argument("--config", default=None)
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        sys.exit(selftest())
    if not a.config:
        ap.error("需要 --config")
    with open(a.config) as f:
        cfg = json.load(f)
    sys.exit(0 if run_once(cfg) == 0 else 1)

if __name__ == "__main__":
    main()
