#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_server.py — OTA 服务器最小实现（Phase 1，自研三接口）

接口（与计划 3.2 一致，网络/本地同语义）：
  POST /ota/check    req {device_id, platform, app_id, version}
                     resp {upgrade, url, new_version, size, sha256, sign}
  GET  /ota/pkg?id=<file>  完整下载 + HTTP Range 断点续传
  POST /ota/report   req {device_id, platform, app_id, version, result, error}

版本库：--dir 下的 manifest.json + firmware/*.otapkg
  manifest.json:
  {
    "platforms": {
      "2": { "app_id": 0, "latest": "0x01020000",
             "versions": { "0x01020000": {"file": "fw.otapkg", "sha256": "...", "sign": ""} } }
    }
  }

用法：
  python ota_server.py --port 8080 --dir server_data
"""
import argparse, hashlib, json, os, re, sys, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

def load_manifest(dirp):
    p = os.path.join(dirp, "manifest.json")
    if not os.path.exists(p):
        return {"platforms": {}}
    with open(p, "r", encoding="utf-8") as f:
        return json.load(f)

def file_sha256(dirp, name):
    fp = os.path.join(dirp, "firmware", name)
    if not os.path.exists(fp):
        return ""
    h = hashlib.sha256()
    with open(fp, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

class OtaHandler(BaseHTTPRequestHandler):
    server_version = "OTA/0.1"

    def _json(self, code, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_body(self):
        n = int(self.headers.get("Content-Length", "0") or "0")
        return self.rfile.read(n) if n > 0 else b""

    # ---- POST /ota/check ----
    def do_POST(self):
        if self.path.startswith("/ota/trigger"):
            self.handle_trigger()
        elif self.path.startswith("/ota/check"):
            self.handle_check()
        elif self.path.startswith("/ota/report"):
            self.handle_report()
        else:
            # v9.81i: device upload POST to configured http.url; deliver pending ota_update
            self.handle_device_post()

    # ---- POST /ota/trigger -- set pending command (delivered on next device upload) ----
    def handle_trigger(self):
        try:
            req = json.loads(self._read_body().decode("utf-8") or "{}")
        except Exception:
            self._json(400, "{""error"": ""bad json""}"); return
        url = req.get("url", "")
        if not url:
            self._json(400, "{""error"": ""url required""}"); return
        self.server.pending_cmd = {"command_type": "ota_update", "command_data": url}
        self._json(200, {"ok": True, "pending": self.server.pending_cmd})

    # ---- device upload POST (any path) -- deliver pending cmd or empty ----
    def handle_device_post(self):
        self._read_body()
        cmd = getattr(self.server, "pending_cmd", None)
        if cmd:
            self.server.pending_cmd = None   # one-shot
            print("[ota] send cmd:", cmd)
            self._json(200, {"code": 0, "data": cmd})
        else:
            self._json(200, {"code": 0})
    def handle_check(self):
        try:
            req = json.loads(self._read_body().decode("utf-8") or "{}")
        except Exception:
            self._json(400, {"error": "bad json"})
            return
        platform = str(req.get("platform", "2"))
        app_id = int(req.get("app_id", 0))
        cur_ver = req.get("version", 0)
        if isinstance(cur_ver, str):
            try: cur_ver = int(cur_ver, 0)
            except ValueError: cur_ver = 0

        man = self.server.manifest["platforms"].get(platform)
        if not man or int(man.get("app_id", 0)) != app_id:
            self._json(200, {"upgrade": False})
            return
        latest = man.get("latest")
        if not latest or latest not in man["versions"]:
            self._json(200, {"upgrade": False})
            return
        try:
            latest_int = int(latest, 0)
        except ValueError:
            latest_int = 0

        if latest_int <= int(cur_ver):
            self._json(200, {"upgrade": False})
            return

        ver = man["versions"][latest]
        name = ver["file"]
        fp = os.path.join(self.server.root, "firmware", name)
        size = os.path.getsize(fp) if os.path.exists(fp) else 0
        resp = {
            "upgrade": True,
            "url": f"/ota/pkg?id={name}",
            "new_version": "0x%08X" % latest_int,
            "size": size,
            "sha256": ver.get("sha256") or file_sha256(self.server.root, name),
            "sign": ver.get("sign", ""),
        }
        self._json(200, resp)

    # ---- POST /ota/report ----
    def handle_report(self):
        try:
            req = json.loads(self._read_body().decode("utf-8") or "{}")
        except Exception:
            self._json(400, {"error": "bad json"})
            return
        # 记录到 reports.log（时间戳 + JSON）
        log = os.path.join(self.server.root, "reports.log")
        line = json.dumps({"ts": time.strftime("%Y-%m-%d %H:%M:%S"), **req}, ensure_ascii=False)
        with open(log, "a", encoding="utf-8") as f:
            f.write(line + "\n")
        self._json(200, {"ok": True})

    # ---- GET /ota/pkg?id=<file> （支持 Range 断点续传）----
    def do_GET(self):
        if not self.path.startswith("/ota/pkg"):
            self._json(404, {"error": "not found"})
            return
        import urllib.parse
        q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        name = (q.get("id") or [""])[0]
        # 防目录穿越
        base = os.path.basename(name)
        fp = os.path.join(self.server.root, "firmware", base)
        if not os.path.exists(fp):
            self._json(404, {"error": "pkg not found"})
            return
        size = os.path.getsize(fp)

        # Range 解析：bytes=N- / bytes=N-M
        start = 0
        rng = self.headers.get("Range")
        if rng:
            m = re.match(r"bytes=(\d+)-", rng)
            if m:
                start = int(m.group(1))
                if start >= size:
                    self.send_response(416)
                    self.send_header("Content-Range", f"bytes */{size}")
                    self.end_headers()
                    return

        self.send_response(206 if rng else 200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(size - start))
        self.send_header("Accept-Ranges", "bytes")
        if rng:
            self.send_header("Content-Range", f"bytes {start}-{size-1}/{size}")
        self.send_header("Content-Disposition", f'attachment; filename="{base}"')
        self.end_headers()
        with open(fp, "rb") as f:
            f.seek(start)
            remaining = size - start
            while remaining > 0:
                chunk = f.read(min(65536, remaining))
                if not chunk:
                    break
                self.wfile.write(chunk)
                remaining -= len(chunk)

    def log_message(self, fmt, *args):
        pass  # 安静模式

def main():
    ap = argparse.ArgumentParser(description="OTA 服务器（check/pkg+Range/report）")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--dir", default="server_data", help="版本库目录（含 manifest.json + firmware/）")
    a = ap.parse_args()

    root = os.path.abspath(a.dir)
    os.makedirs(os.path.join(root, "firmware"), exist_ok=True)
    man = load_manifest(root)
    # 自动补齐 sha256
    for plat, m in man.get("platforms", {}).items():
        for ver, info in m.get("versions", {}).items():
            if not info.get("sha256"):
                info["sha256"] = file_sha256(root, info["file"])
    manifest_path = os.path.join(root, "manifest.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(man, f, ensure_ascii=False, indent=2)

    srv = ThreadingHTTPServer(("0.0.0.0", a.port), OtaHandler)
    srv.root = root
    srv.manifest = man
    srv.pending_cmd = None
    print(f"OTA server @ http://0.0.0.0:{a.port}  (dir: {root})")
    print(f"  platforms: {list(man.get('platforms', {}).keys())}")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
