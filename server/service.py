#!/usr/bin/env python3
"""
service.py — orbis auxiliary service
Listens on :8002, proxied by nginx at /svc/
Communicates with the orbis REST API on :8001
"""

import http.server
import urllib.request
import urllib.error
import json
import re
import os
import signal
import sys
import threading

API_HOST = "http://127.0.0.1:8001"
COOKIES_FILE = os.path.join(os.path.dirname(__file__), "cookies")
SERVICE_PORT = 8002


def read_session_cookie():
    """Read session cookie from the orbis cookies file."""
    try:
        with open(COOKIES_FILE) as f:
            for line in f:
                m = re.search(r'session\s+(\S+)', line)
                if m:
                    return "session=" + m.group(1)
    except FileNotFoundError:
        pass
    return None


def api_request(method, path, body=None, cookie=None):
    """Make a request to the orbis REST API."""
    url = API_HOST + path
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)

    req.add_header("Content-Type", "application/json")
    if cookie:
        req.add_header("Cookie", cookie)

    try:
        with urllib.request.urlopen(req) as resp:
            raw = resp.read()
            return resp.status, raw.decode() if raw else None
    except urllib.error.HTTPError as e:
        raw = e.read()
        return e.code, raw.decode() if raw else None


class ServiceHandler(http.server.BaseHTTPRequestHandler):

    def log_message(self, fmt, *args):
        pass  # Silence default access log — nginx handles it

    def send_json(self, status, body):
        encoded = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def send_error_json(self, status, title, issue=""):
        self.send_json(status, {"title": title, "issue": issue})

    def read_body(self):
        length = int(self.headers.get("Content-Length", 0))
        if length:
            raw = self.rfile.read(length)
            try:
                return json.loads(raw)
            except json.JSONDecodeError:
                return None
        return None

    def get_cookie(self):
        """Forward the client cookie or fall back to the cached session cookie."""
        cookie = self.headers.get("Cookie")
        if cookie and "session=" in cookie:
            return cookie
        return SESSION_COOKIE

    def handle_route(self, method):
        # Strip /svc prefix
        path = self.path
        if path.startswith("/svc"):
            path = path[4:] or "/"

        cookie = self.get_cookie()
        body = self.read_body() if method in ("POST", "PUT", "PATCH") else None

        # ── Route: GET /svc/ping ──────────────────────────
        if method == "GET" and path == "/ping":
            self.send_json(200, {"status": "ok"})
            return

        # ── Route: GET /svc/users ─────────────────────────
        if method == "GET" and path.startswith("/users"):
            status, data = api_request("GET", "/api/users", cookie=cookie)
            if status == 200:
                self.send_json(200, json.loads(data))
            else:
                self.send_error_json(status, "API error", data or "")
            return

        self.send_error_json(404, "Not Found", "Endpoint not found")

    def do_GET(self):    self.handle_route("GET")
    def do_POST(self):   self.handle_route("POST")
    def do_PUT(self):    self.handle_route("PUT")
    def do_PATCH(self):  self.handle_route("PATCH")
    def do_DELETE(self): self.handle_route("DELETE")


if __name__ == "__main__":
    SESSION_COOKIE = read_session_cookie()
    port = int(sys.argv[1]) if len(sys.argv) > 1 else SERVICE_PORT
    server = http.server.HTTPServer(("127.0.0.1", port), ServiceHandler)

    def handle_sigterm(signum, frame):
        threading.Thread(target=server.shutdown).start()

    signal.signal(signal.SIGTERM, handle_sigterm)

    print(f"Service waiting on port {port}")
    server.serve_forever()
