#!/usr/bin/env python3
"""HTTP front for potahto_sim: serves the web app and forwards POST /api to the
REAL protocol code (compiled for the host). Port 8080."""
import http.server, subprocess, urllib.parse, threading, sys, os
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "webapp", "index.html")
sim = subprocess.Popen([os.path.join(os.path.dirname(__file__), "potahto_sim")], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
lock = threading.Lock()
def ask(mode, pin, body):
    with lock:
        sim.stdin.write(f"{mode}\t{pin}\t{body}\n"); sim.stdin.flush()
        return sim.stdout.readline().strip()
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def _send(self, code, body, ctype="application/json"):
        b = body.encode() if isinstance(body, str) else body
        self.send_response(code); self.send_header("Content-Type", ctype)
        self.send_header("Access-Control-Allow-Origin", "*"); self.send_header("Content-Length", str(len(b)))
        self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        if self.path in ("/", "/index.html"): self._send(200, open(SRC, "rb").read(), "text/html")
        else: self._send(404, "not found", "text/plain")
    def do_POST(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query)
        body = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
        pin = (q.get("pin") or [""])[0] or self.headers.get("X-Pin", "")
        if u.path == "/api": self._send(200, ask("http", pin, body.replace("\n", " ")))
        elif u.path == "/update": self._send(200 if pin == "123456" else 403, '{"ok":true}' if pin == "123456" else '{"ok":false,"err":"auth"}')
        else: self._send(404, "{}")
http.server.ThreadingHTTPServer(("127.0.0.1", 8080), H).serve_forever()
