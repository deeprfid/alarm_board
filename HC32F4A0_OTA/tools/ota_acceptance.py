#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_acceptance.py — 统一 OTA 全链路验收（软件层，一键运行）

串联全部可自动验证项（对应 doc/OTA测试矩阵.md）：
  A. 打包 → 版本库
  B. 服务器三接口（check/pkg+Range/report）
  C. 上位机工具（check/download+断点续传）
  D. 设备端模拟器（全新/续传/半扇区/ACK丢失）
  E. USB 通道 feed 状态机
  F. RK Agent（原子替换/回滚）
退出码：0=全部通过，1=有失败
"""
import os, subprocess, sys, tempfile, threading, time

TOOLS = os.path.dirname(os.path.abspath(__file__))
APP_HEX = os.path.join(os.path.dirname(TOOLS), "hc32f4a0_app", "projects", "MDK", "output", "firmware_p4.hex")
SERVER_DATA = os.path.join(TOOLS, "server_data")
PORT = 8080

def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def run(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd or TOOLS, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return r.returncode, (r.stdout + r.stderr)

def section(name):
    print(f"\n===== {name} =====")

results = []
def record(name, ok, detail=""):
    results.append((name, ok))
    print(f"[{'PASS' if ok else 'FAIL'}] {name}" + (f"  {detail}" if detail and not ok else ""))

def _mkdtemp(prefix):
    # 兼容受限环境：tempfile.mkdtemp 创建的目录在某些沙箱下不可写，改用 makedirs + uuid
    import uuid
    base = os.path.abspath(os.environ.get("OTA_ACC_TMP", "."))
    d = os.path.join(base, prefix + uuid.uuid4().hex)
    os.makedirs(d, exist_ok=False)
    return d

def main():
    ep()
    tmp = _mkdtemp("ota_acc_")
    try:
        # ---- A. 打包 → 版本库 ----
        section("A. 打包 + 版本库")
        pkg = os.path.join(tmp, "fw_acc.otapkg")
        rc, out = run(["python", "ota_pack.py", APP_HEX, "--version", "0x01020000", "--platform", "2", "--app", "0", "-o", pkg])
        record("A1 打包 OTA 包", rc == 0 and os.path.exists(pkg), out[-300:])
        # 注意：add 到临时目录，避免污染正式版本库（正式库打包由 build_ota.py/ota_pack 负责）
        acc_dir = os.path.join(tmp, "acc_server_data")
        rc, out = run(["python", "ota_upgrade_tool.py", "add", pkg, "--platform", "2", "--version", "0x01020000", "--app", "0", "--dir", acc_dir])
        record("A2 加入版本库(临时)", rc == 0, out[-300:])

        # ---- B. 服务器三接口 ----
        section("B. 服务器三接口")
        import ota_server
        from http.server import ThreadingHTTPServer
        srv = ThreadingHTTPServer(("127.0.0.1", PORT), ota_server.OtaHandler)
        srv.root = os.path.abspath(SERVER_DATA)
        srv.manifest = ota_server.load_manifest(os.path.abspath(SERVER_DATA))
        t = threading.Thread(target=srv.serve_forever, daemon=True)
        t.start()
        time.sleep(0.5)
        rc, out = run(["python", "ota_server_selftest.py"])
        record("B 服务器三接口自测", rc == 0, out[-300:])

        # ---- C. 上位机工具 ----
        section("C. 上位机工具（网络升级）")
        dl = os.path.join(tmp, "dl.otapkg")
        rc, out = run(["python", "ota_upgrade_tool.py", "check", "--server", f"http://127.0.0.1:{PORT}", "--platform", "2", "--app", "0", "--version", "0"])
        record("C1 check 有新版", rc == 0 and "有新版" in out, out[-300:])
        rc, out = run(["python", "ota_upgrade_tool.py", "download", "--server", f"http://127.0.0.1:{PORT}", "--platform", "2", "--app", "0", "--version", "0", "--out", dl])
        record("C2 download+校验", rc == 0 and "校验通过" in out, out[-300:])
        # 断点续传
        # 断点源取 C2 已校验通过的 dl.otapkg（与服务器 latest 严格一致）
        # （旧代码读残留 fw_01020000.otapkg 旧包半包 + 服务器新包 → 内容混搭 → 校验失败）
        data = open(dl, "rb").read()
        half = os.path.join(tmp, "dl2.otapkg")
        with open(half, "wb") as f:
            f.write(data[:len(data)//2])
        rc, out = run(["python", "ota_upgrade_tool.py", "download", "--server", f"http://127.0.0.1:{PORT}", "--platform", "2", "--app", "0", "--version", "0", "--out", half])
        ok = rc == 0 and open(half, "rb").read() == data
        record("C3 download 断点续传", ok, out[-300:])
        srv.shutdown()

        # ---- D. 设备端模拟器 ----
        section("D. 设备端模拟（ota_selftest）")
        import ota_selftest
        pkg_data = open(pkg, "rb").read()
        try:
            ota_selftest.run_scenario("全新下载", pkg_data)
            ota_selftest.run_scenario("中断续传(10KB)", pkg_data, initial_progress=10240)
            ota_selftest.run_scenario("中断续传(半扇区)", pkg_data, initial_progress=5000)
            ota_selftest.run_scenario("ACK丢失", pkg_data, drop_ack=True)
            record("D 设备端模拟 4 场景", True)
        except Exception as e:
            record("D 设备端模拟 4 场景", False, str(e))

        # ---- E. USB feed 状态机 ----
        section("E. USB 通道 feed 状态机")
        rc, out = run(["python", "_feed_acc.py"])
        record("E feed 状态机", rc == 0, out[-300:])

        # ---- F. RK Agent ----
        section("F. RK Agent")
        rc, out = run(["python", "rk_ota_agent.py", "--selftest"])
        record("F RK Agent 自测", rc == 0 and "全部通过" in out, out[-300:])
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)

    print("\n" + "=" * 50)
    fails = [n for n, ok in results if not ok]
    for n, ok in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {n}")
    print(f"\n结果: {len(results)-len(fails)}/{len(results)} 通过")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
