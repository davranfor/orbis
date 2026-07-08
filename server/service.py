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
import logging
import signal
import sys
import threading

SERVICE_PORT = 8002


logging.basicConfig(
    level=logging.ERROR,
    format="%(asctime)s %(levelname)s %(message)s",
)



def api_request(method, path, body=None, cookie=None, host=None):
    """Make a request to the orbis REST API."""
    url = host + path
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
    except urllib.error.URLError as e:
        logging.error("API unreachable %s %s: %s", method, url, e.reason)
        return 503, None


class ServiceHandler(http.server.BaseHTTPRequestHandler):

    API_HOST = "http://127.0.0.1:8001"

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
        """Forward the client cookie to the REST API."""
        return self.headers.get("Cookie")

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
            status, data = api_request("GET", "/api/users", cookie=cookie, host=self.API_HOST)
            if status == 200:
                self.send_json(200, json.loads(data))
            else:
                self.send_error_json(status, "API error", data or "")
            return

        logging.error("Unhandled route: %s %s", method, self.path)
        self.send_error_json(404, "Not Found", "Endpoint not found")

    def do_GET(self):    self.handle_route("GET")
    def do_POST(self):   self.handle_route("POST")
    def do_PUT(self):    self.handle_route("PUT")
    def do_PATCH(self):  self.handle_route("PATCH")
    def do_DELETE(self): self.handle_route("DELETE")


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else SERVICE_PORT
    ServiceHandler.API_HOST = sys.argv[2] if len(sys.argv) > 2 else "http://127.0.0.1:8001"
    server = http.server.HTTPServer(("127.0.0.1", port), ServiceHandler)

    def handle_sigterm(signum, frame):
        threading.Thread(target=server.shutdown).start()

    signal.signal(signal.SIGTERM, handle_sigterm)

    print(f"Service waiting on port {port}")
    server.serve_forever()
