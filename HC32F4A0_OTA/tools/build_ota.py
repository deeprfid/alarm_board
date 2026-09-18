#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_ota.py — 一键构建 + 发布 + 验收（CI 友好）

流程：
  1. 编译 App（UV4，0E/0W）
  2. 编译 Bootloader（UV4，0E/0W）
  3. fromelf 生成 App hex
  4. 体积监控（fw_size_monitor）
  5. 打包 OTA 包 + 加入版本库（ota_upgrade_tool add）
  6. 合并烧录 hex（merge_hex boot+app）
  7. 全链路验收（ota_acceptance）

用法：
  python build_ota.py                      # 全流程
  python build_ota.py --skip-build         # 跳过编译（只做打包+验收）
  python build_ota.py --skip-acceptance    # 跳过验收
退出码：0=全部通过
"""
import argparse, os, re, subprocess, sys, time

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
UV4 = r"D:\Keil_v5\UV4\UV4.exe"
APP_PROJ = os.path.join(ROOT, "hc32f4a0_app", "projects", "MDK", "hc32f4a0_app.uvprojx")
BOOT_PROJ = os.path.join(ROOT, "hc32f4a0_boot", "projects", "MDK", "hc32f4a0_boot.uvprojx")
APP_AXF = os.path.join(ROOT, "hc32f4a0_app", "projects", "MDK", "output", "firmware.axf")
APP_MAP = os.path.join(ROOT, "hc32f4a0_app", "projects", "MDK", "output", "firmware.map")
APP_HEX = os.path.join(ROOT, "hc32f4a0_app", "projects", "MDK", "output", "firmware_p4.hex")
BOOT_HEX = os.path.join(ROOT, "hc32f4a0_boot", "projects", "MDK", "output", "hc32f4a0_boot.hex")
MERGED = os.path.join(TOOLS, "merged_f4a0.hex")
PKG = os.path.join(TOOLS, "server_data", "firmware", "fw_01020000.otapkg")
VERSION = "0x01020000"

def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def sh(cmd, cwd=None, timeout=300):
    r = subprocess.run(cmd, cwd=cwd or TOOLS, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=timeout)
    return r.returncode, (r.stdout or "") + (r.stderr or "")

def build_uv(proj, name):
    log = os.path.join(ROOT, f"build_{name}.log")
    rc, _ = sh([UV4, "-r", proj, "-o", log, "-j0"])
    if rc != 0:
        rc = 0  # UV4 返回 0/1/2；实际结果看 log
    # 轮询等待完成（UV4 同步调用时 -r 会等待）
    try:
        with open(log, "r", encoding="utf-8", errors="replace") as f:
            txt = f.read()
        m = re.search(r"(\d+) Error\(s\), (\d+) Warning\(s\)", txt)
        if m:
            errs, warns = int(m.group(1)), int(m.group(2))
            print(f"  {name}: {errs}E/{warns}W")
            return errs == 0
    except FileNotFoundError:
        pass
    print(f"  {name}: 构建状态未知（检查 {log}）")
    return False

def main():
    ep()
    ap = argparse.ArgumentParser(description="一键构建+发布+验收")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--skip-acceptance", action="store_true")
    a = ap.parse_args()

    ok = True
    print("===== OTA 一键构建/发布/验收 =====")

    # 1-2) 编译
    if not a.skip_build:
        print("[1/7] 编译 App...")
        ok &= build_uv(APP_PROJ, "app_ota")
        print("[2/7] 编译 Bootloader...")
        ok &= build_uv(BOOT_PROJ, "boot_ota")
        if not os.path.exists(APP_AXF):
            print("  App axf 缺失！"); ok = False
    else:
        print("[1-2/7] 跳过编译")

    # 3) App hex
    print("[3/7] 生成 App hex...")
    if os.path.exists(APP_AXF):
        rc, _ = sh([r"D:\Keil_v5\ARM\ARMCC\bin\fromelf.exe", "--i32", "--output=" + APP_HEX, APP_AXF])
        if rc != 0 or not os.path.exists(APP_HEX):
            print("  fromelf 失败"); ok = False
        else:
            print(f"  app hex: {os.path.getsize(APP_HEX)}B")

    # 4) 体积监控
    print("[4/7] 体积监控...")
    if os.path.exists(APP_MAP):
        rc, out = sh(["python", "fw_size_monitor.py", "--map", APP_MAP])
        print(out.strip())
        if rc != 0:
            print("  [注意] 体积预警/超限"); ok &= (rc == 0 or rc == 1)
    else:
        print("  map 缺失"); ok = False

    # 5) 打包 + 版本库
    print("[5/7] 打包 + 版本库...")
    if os.path.exists(APP_HEX):
        rc, out = sh(["python", "ota_pack.py", APP_HEX, "--version", VERSION, "--platform", "2", "--app", "0", "-o", PKG])
        if rc == 0:
            rc2, out2 = sh(["python", "ota_upgrade_tool.py", "add", PKG, "--platform", "2", "--version", VERSION, "--app", "0", "--dir", os.path.join(TOOLS, "server_data")])
            ok &= (rc2 == 0)
            print("  版本库已更新" if rc2 == 0 else "  add 失败")
        else:
            print("  打包失败"); ok = False
    else:
        print("  hex 缺失"); ok = False

    # 6) 合并烧录 hex
    print("[6/7] 合并烧录 hex...")
    if os.path.exists(BOOT_HEX) and os.path.exists(APP_HEX):
        rc, out = sh(["python", "merge_hex.py", BOOT_HEX, APP_HEX, "-o", MERGED])
        ok &= (rc == 0)
        print(out.strip() if rc == 0 else "  合并失败")
    else:
        print("  boot/app hex 缺失"); ok = False

    # 7) 验收
    if not a.skip_acceptance:
        print("[7/7] 全链路验收...")
        rc, out = sh(["python", "ota_acceptance.py"])
        print(out[-400:])
        ok &= (rc == 0)
    else:
        print("[7/7] 跳过验收")

    print("\n===== " + ("全部通过 ✔" if ok else "存在问题 ✘") + " =====")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
