#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_keygen.py — OTA HMAC 密钥生成/管理（正式化）

生成 32 字节 HMAC-SHA256 密钥，输出：
  - 密钥文件（hex，32 字节）—— 分发给打包工具/服务器/上位机
  - C 头文件片段 —— 粘贴进设备端 ota_security.c（OTA_SEC_KEY）

用法：
  python ota_keygen.py -o key_2026.bin                 # 生成新密钥
  python ota_keygen.py --show key_2026.bin             # 显示密钥（hex）
  python ota_keygen.py --c-header key_2026.bin -o ota_key.h   # 生成 C 头
安全：
  - 密钥文件权限 0600；不要提交到 git（.gitignore 加 *.key / key_*.bin）
  - 轮换：重新生成 → 新固件用新 key 打包 → 设备端换 OTA_SEC_KEY 编译
"""
import argparse, hashlib, os, secrets, sys

def ep():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

def show_key(path):
    with open(path, "rb") as f:
        k = f.read()
    if len(k) != 32:
        sys.exit(f"密钥必须 32 字节（当前 {len(k)}）")
    print(f"密钥文件: {path} ({len(k)}B)")
    print(f"HEX:      {k.hex().upper()}")
    print(f"SHA256:   {hashlib.sha256(k).hexdigest()}")
    return k

def c_header(k, name="OTA_SEC_KEY"):
    lines = ["/* OTA_SEC_KEY 替换片段：粘贴进 hc32f4a0_app/projects/app/src/ota_security.c */",
             f"static const uint8_t {name}[] = {{"]
    for i in range(0, 32, 8):
        row = ", ".join(f"0x{b:02X}" for b in k[i:i+8])
        lines.append("    " + row + ("," if i + 8 < 32 else ""))
    lines.append("};")
    return "\n".join(lines)

def main():
    ep()
    ap = argparse.ArgumentParser(description="OTA 密钥管理")
    ap.add_argument("-o", "--output", default=None, help="生成密钥文件（32B）")
    ap.add_argument("--show", default=None, help="显示密钥文件内容")
    ap.add_argument("--c-header", default=None, help="从密钥文件生成 C 头片段")
    a = ap.parse_args()

    if a.show:
        show_key(a.show)
        return
    if a.c_header:
        k = show_key(a.c_header)
        out = a.output or "ota_key.h"
        with open(out, "w") as f:
            f.write(c_header(k) + "\n")
        print(f"C 头片段 -> {out}")
        return
    if a.output:
        k = secrets.token_bytes(32)
        with open(a.output, "wb") as f:
            f.write(k)
        os.chmod(a.output, 0o600)
        print(f"新密钥已生成: {a.output} ({len(k)}B)")
        print(f"HEX: {k.hex().upper()}")
        print(f"注意: 密钥文件不要提交 git；正式使用需安全分发（产线/服务器/设备编译）")
        return
    ap.error("需要 -o（生成）/ --show（显示）/ --c-header（C 头）")

if __name__ == "__main__":
    main()
