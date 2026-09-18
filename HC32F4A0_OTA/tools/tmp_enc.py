# -*- coding: utf-8 -*-
import os, glob, collections

ROOT = r"J:\dsh\HC32F4A020260320_RYK"
os.chdir(ROOT)

def classify(data):
    if len(data) == 0:
        return "empty"
    try:
        data.decode("utf-8")
        return "utf8" if any(b >= 0x80 for b in data) else "ascii"
    except UnicodeDecodeError:
        pass
    try:
        data.decode("gbk")
        return "gbk"
    except UnicodeDecodeError:
        try:
            data.decode("gb18030")
            return "gb18030"
        except UnicodeDecodeError:
            return "UNDECODABLE"

stats = collections.Counter()
bad = []
for root in ["hc32f4a0_app", "hc32f4a0_driver", "tools", "hc32f4a0_boot"]:
    for p in glob.glob(root + "/**/*", recursive=True):
        if not os.path.isfile(p):
            continue
        ext = os.path.splitext(p)[1].lower()
        if ext not in (".c", ".h", ".py", ".md", ".txt", ".uvprojx"):
            continue
        with open(p, "rb") as f:
            data = f.read()
        c = classify(data)
        stats[c] += 1
        if c in ("UNDECODABLE", "gb18030"):
            bad.append((c, p))
print("STATS:", dict(stats))
print("---- problem files ----")
for c, p in bad:
    print("%-20s %s" % (c, p))
