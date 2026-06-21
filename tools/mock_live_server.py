"""Local stand-in for a master's GET /api/live — test the Live view without
hardware/WiFi. Run it, then in the app's Live page set host to 127.0.0.1:8080
and Connect. Each wheel animates a moving hot band so the shared colour scale,
legend, and rounded patch are visible. Dev tool — not shipped."""

from __future__ import annotations

import json
import math
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

COLS, ROWS = 9, 5
WHEELS = ["FL", "FR", "RL", "RR"]
# Per-wheel base temp so one corner clearly runs hotter than the rest.
BASE = {"FL": 55.0, "FR": 70.0, "RL": 60.0, "RR": 90.0}


def grid(base: float, t: float) -> list[float]:
    out = []
    hot_col = (COLS - 1) * (0.5 + 0.5 * math.sin(t))  # hot band sweeps the tread
    for r in range(ROWS):
        for c in range(COLS):
            edge = abs(c - hot_col)
            row_falloff = abs(r - ROWS // 2) * 2.0
            out.append(round(base + 18.0 * math.exp(-(edge**2) / 4.0) - row_falloff, 1))
    return out


class Handler(BaseHTTPRequestHandler):
    """Serve a synthetic /api/live frame for each request."""

    def do_GET(self):
        if self.path != "/api/live":
            self.send_error(404)
            return
        t = time.time()
        payload = {
            "cols": COLS,
            "rows": ROWS,
            "wheels": {
                w: {"temps": grid(BASE[w], t + i)} for i, w in enumerate(WHEELS)
            },
        }
        body = json.dumps(payload).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_a):
        pass


if __name__ == "__main__":
    srv = HTTPServer(("127.0.0.1", 8080), Handler)
    print("Mock live server on http://127.0.0.1:8080/api/live — Ctrl+C to stop")
    srv.serve_forever()
