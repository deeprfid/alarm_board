#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ota_server 三接口自测：check / pkg(完整+Range续传) / report"""
import hashlib, json, sys, urllib.request

BASE = "http://127.0.0.1:8080"

def post(p, obj):
    req = urllib.request.Request(BASE + p, data=json.dumps(obj).encode(), method="POST",
                                  headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return r.status, json.loads(r.read().decode())

def get(p, headers=None):
    req = urllib.request.Request(BASE + p, headers=headers or {})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.status, r.read(), dict(r.headers)

def sha(b):
    return hashlib.sha256(b).hexdigest()

# 1) check 旧版本 → 有升级
st, r = post("/ota/check", {"device_id": "dev01", "platform": 2, "app_id": 0, "version": 0})
assert st == 200 and r["upgrade"] is True, r
url = r["url"]; size = r["size"]; ref_sha = r["sha256"]
cur_ver = r["new_version"]   # 服务器最新版本（动态，勿写死）
print(f"[PASS] check(旧版) upgrade=true new={cur_ver} size={size}")

# 2) check 当前版本 → 无升级（用服务器 latest 动态判断）
st, r = post("/ota/check", {"device_id": "dev01", "platform": 2, "app_id": 0, "version": cur_ver})
assert st == 200 and r["upgrade"] is False, r
print("[PASS] check(当前版) upgrade=false")

# 3) pkg 完整下载 → SHA256 一致
st, data, hdrs = get(url)
assert st == 200 and len(data) == size, (st, len(data), size)
assert sha(data) == ref_sha, "完整下载 SHA 不一致"
print(f"[PASS] pkg 完整下载 {len(data)}B sha={sha(data)[:12]}")

# 4) pkg Range 断点续传（从中间续传，拼接后 SHA 一致）
mid = size // 2
st1, d1, _ = get(url)
st2, d2, h2 = get(url, {"Range": f"bytes={mid}-"})
assert st2 == 206, st2
assert h2.get("Content-Range") == f"bytes {mid}-{size-1}/{size}", h2.get("Content-Range")
joined = d1[:mid] + d2
assert len(joined) == size and sha(joined) == ref_sha, "Range 续传拼接不一致"
print(f"[PASS] pkg Range续传 0..{mid} + {mid}..end = {len(joined)}B sha={sha(joined)[:12]}")

# 5) report
st, r = post("/ota/report", {"device_id": "dev01", "platform": 2, "app_id": 0,
                             "version": "0x01020000", "result": 0, "error": ""})
assert st == 200 and r.get("ok") is True, r
print("[PASS] report ok=True")

# 6) 404
try:
    get("/ota/pkg?id=../secret")
    raise AssertionError("目录穿越未拦截")
except urllib.error.HTTPError as e:
    assert e.code == 404
print("[PASS] 目录穿越拦截 404")

print("\n全部通过：check / pkg(完整+Range) / report / 安全")
