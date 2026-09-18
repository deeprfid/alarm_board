#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_release.py - one-click firmware release generation (version source: FW_VERSION_NUM)

Usage (after the 3 KEIL projects are compiled):
    python tools/build_release.py
    python tools/build_release.py --build    (also compiles driver/boot/app via UV4)

Outputs to release/ - v9.82e: filenames carry a timestamp so builds are distinguishable:
    merged_f4a0_v<ver>_<YYYYMMDD_HHMM>.bin      DAP-LINK/J-Link flash (boot@0x0 + app@0x10000)
    fw_0x<ver>_<YYYYMMDD_HHMM>.otapkg           serial/USB-CDC/HTTP channel upgrade package
    fw_0x<ver+0x100>_<YYYYMMDD_HHMM>.otapkg    bridge package (same payload, higher header ver)
    FW.BIN                                     USB virtual U-disk (fixed name, byte-identical to pkg)
    manifest.txt                               version/time/size/SHA256 (always points to the latest)

Self-check: reads FW_VERSION_NUM, validates package header versions.
Historical timestamped artifacts are KEPT (traceable); only legacy non-timestamped files are cleaned.
"""
import os, re, sys, hashlib, subprocess, shutil, datetime

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP_HEX  = os.path.join(ROOT, "hc32f4a0_app", "projects", "MDK", "output", "firmware.hex")
BOOT_HEX = os.path.join(ROOT, "hc32f4a0_boot", "projects", "MDK", "output", "hc32f4a0_boot.hex")
VHDR     = os.path.join(ROOT, "driver_lib", "hc32f46_driver.h")
RELEASE  = os.path.join(ROOT, "release")
MERGE_BIN = os.path.join(ROOT, "tools", "merge_bin.py")
OTA_PACK  = os.path.join(ROOT, "tools", "ota_pack.py")
MERGED_NAME = "merged_f4a0_v981cl.bin"   # legacy non-timestamped name (cleanup reference only)

# UV4 one-click build (set env KEIL_UV4 to override the default path)
UV4 = os.environ.get("KEIL_UV4", r"D:\Keil_v5\UV4\UV4.exe")
PROJECTS = [
    ("driver", os.path.join(ROOT, "hc32f4a0_driver", "projects", "MDK", "hc32f4a0_driver.uvprojx")),
    ("boot",   os.path.join(ROOT, "hc32f4a0_boot", "projects", "MDK", "hc32f4a0_boot.uvprojx")),
    ("app",    os.path.join(ROOT, "hc32f4a0_app", "projects", "MDK", "hc32f4a0_app.uvprojx")),
]


def get_version():
    txt = open(VHDR, encoding="gbk", errors="replace").read()
    m = re.search(r"#define\s+FW_VERSION_NUM\s+(0x[0-9A-Fa-f]+)", txt)
    if not m:
        sys.exit(f"FW_VERSION_NUM not found: {VHDR}")
    return int(m.group(1), 16)


def run(cmd):
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"command failed: {' '.join(cmd)}\n{r.stdout}{r.stderr}")
    return r.stdout


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blk in iter(lambda: f.read(65536), b""):
            h.update(blk)
    return h.hexdigest()


def pkg_version(p):
    b = open(p, "rb").read(8)
    if len(b) >= 8 and b[0:4] == b"OTA1":
        return int.from_bytes(b[4:8], "little")
    return None


def build_all():
    """Compile the 3 KEIL projects via UV4 (driver -> boot -> app), exit on failure"""
    if not os.path.exists(UV4):
        sys.exit(f"UV4.exe not found: {UV4} (set env KEIL_UV4)")
    for name, proj in PROJECTS:
        if not os.path.exists(proj):
            sys.exit(f"project missing: {proj}")
        log = os.path.join(ROOT, f"_uv4_{name}.log")
        print(f"[build] compiling {name} ...")
        r = subprocess.run([UV4, "-b", proj, "-j0", "-o", log],
                           cwd=os.path.dirname(proj), capture_output=True, text=True)
        t = ""
        if os.path.exists(log):
            t = open(log, encoding="utf-8", errors="replace").read()
        if "0 Error(s)" not in t:
            sys.exit(f"[build] {name} failed\n{t[-800:]}")
    print("[build] all 3 projects compiled")


def main():
    if "--build" in sys.argv:
        build_all()
    ver = get_version()
    verstr = f"0x{ver:08X}"
    bridge_ver = ver + 0x100
    bridge_str = f"0x{bridge_ver:08X}"
    for p, what in ((APP_HEX, "app firmware.hex"), (BOOT_HEX, "boot hex")):
        if not os.path.exists(p):
            sys.exit(f"missing {what}: {p} (compile the 3 KEIL projects first)")
    os.makedirs(RELEASE, exist_ok=True)

    print(f"=== release generation  ver={verstr} (single source FW_VERSION_NUM) ===")
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M")   # v9.82e: timestamp naming
    merged = os.path.join(RELEASE, f"merged_f4a0_{verstr}_{stamp}.bin")
    pkg    = os.path.join(RELEASE, f"fw_{verstr}_{stamp}.otapkg")
    bridge = os.path.join(RELEASE, f"fw_{bridge_str}_{stamp}.otapkg")
    fwbin  = os.path.join(RELEASE, "FW.BIN")

    # 1) DAP-LINK merged bin
    run(["python", MERGE_BIN, BOOT_HEX, APP_HEX, "-o", merged])
    # 2) upgrade package (3 channels)
    run(["python", OTA_PACK, APP_HEX, "--version", verstr, "--platform", "2", "--app", "0", "-o", pkg])
    # 2b) bridge package (same payload, header ver+0x100)
    run(["python", OTA_PACK, APP_HEX, "--version", bridge_str, "--platform", "2", "--app", "0", "-o", bridge])
    # 3) FW.BIN (USB virtual U-disk, fixed name - device usb_msc_ota looks up "FW.BIN")
    shutil.copyfile(pkg, fwbin)

    # 3b) clean ONLY legacy non-timestamped files (keep all timestamped history)
    removed = []
    for f in sorted(os.listdir(RELEASE)):
        p = os.path.join(RELEASE, f)
        if not os.path.isfile(p) or f in ("manifest.txt", "README.txt"):
            continue
        if ("_" + stamp) in f:
            continue   # this build's fresh artifacts
        if f == "FW.BIN":
            continue
        if f.endswith(".otapkg") or f.endswith(".bin") or f.endswith(".BIN"):
            os.remove(p)
            removed.append(f)
    if removed:
        print("cleaned legacy non-timestamped: " + ", ".join(removed))

    # 4) verify + manifest
    lines = [f"firmware release manifest  ver={verstr}  bridge={bridge_str}  built={stamp}", ""]
    lines.append(f"[LATEST] merged ={os.path.basename(merged)}")
    lines.append(f"         pkg    ={os.path.basename(pkg)}")
    lines.append(f"         bridge ={os.path.basename(bridge)}")
    lines.append("")
    ok = True
    checks = [(os.path.basename(merged), None), (os.path.basename(pkg), ver), (os.path.basename(bridge), bridge_ver), ("FW.BIN", None)]
    for name, exp in checks:
        p = os.path.join(RELEASE, name)
        lines.append(f"{name}  size={os.path.getsize(p)}  sha256={sha256(p)}")
        if exp is not None:
            pv = pkg_version(p)
            if pv != exp:
                ok = False
                lines.append(f"  [FAIL] header ver {pv:#010x} != expected {exp:#010x}")
            else:
                lines.append(f"  [OK]   header ver match {exp:#010x}")
    same = os.path.getsize(pkg) == os.path.getsize(fwbin) and open(pkg, "rb").read() == open(fwbin, "rb").read()
    lines.append("  [OK]   FW.BIN identical to standard otapkg" if same else "  [FAIL] FW.BIN differs from otapkg")
    if not same:
        ok = False
    lines.append("")
    if removed:
        lines.append("cleaned legacy: " + ", ".join(removed))
        lines.append("")
    open(os.path.join(RELEASE, "manifest.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("\n".join(lines))
    if not ok:
        sys.exit("!! verification failed - check build outputs and macros!")
    print(f"\nall generated -> {RELEASE}")


if __name__ == "__main__":
    main()
