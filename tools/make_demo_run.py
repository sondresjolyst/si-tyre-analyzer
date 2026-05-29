"""Generate a synthetic 20-minute, 4-wheel run for demoing the analyzer.

Writes valid .bin files (matching src/storage/LogFormat.h) into ~/SI_Tyre_Analyzer_Runs.
Run: uv run python make_demo_run.py
"""

import math
import os
import random
import struct

COLS, ROWS = 9, 5
RATE_HZ = 2
SCALE = 100
DURATION_S = 20 * 60               # recording longer than the race (lap overlay)
N = DURATION_S * RATE_HZ           # 2400 frames
HDR_FMT = "<IHBBHBBIQ6sI16sI24s18s"
MAGIC = 0x54595254
VERSION = 1
GROUP = 1234
CAR_NAME = "Volvo 242 Turbo"
SESSION_ID = random.randint(1, 50)

# wheel: (enum, base offset °C, hot column 0..8, warm-up time constant s)
WHEELS = {
    "FL": (0, 4.0, 4, 230),    # front, hottest mid
    "FR": (1, 6.0, 7, 210),    # front, hot outer (understeer-ish)
    "RL": (2, -2.0, 1, 260),   # rear, cooler, slight inner
    "RR": (3, 2.0, 6, 240),    # rear, hot outer
}


def frame(t, base, hot_col, tau):
    ambient, peak = 50.0, 88.0 + base
    warm = ambient + (peak - ambient) * (1 - math.exp(-t / tau))
    warm += 3.0 * math.sin(t / 120.0)               # slow stints
    rove = (COLS - 1) * (0.5 + 0.4 * math.sin(t * 0.05))
    cx = (COLS - 1) / 2.0
    g = []
    for r in range(ROWS):
        row_bias = (r - (ROWS - 1) / 2.0) * 1.2
        for c in range(COLS):
            n = (c - cx) / cx
            v = warm + base - 10.0 * n * n + row_bias
            v += 6.0 * math.exp(-((c - hot_col) ** 2) / 3.0)   # tread bias
            v += 10.0 * math.exp(-((c - rove) ** 2) / 4.0)     # roaming spot
            v += random.uniform(-1.5, 1.5)
            g.append(v)
    return g


def write_wheel(folder, name, wp, base, hot_col, tau):
    mac = bytes([0x90, 0x70, 0x69, 0x2A, 0xE6, 0xB0 + wp])
    header = struct.pack(HDR_FMT, MAGIC, VERSION, COLS, ROWS, RATE_HZ, wp,
                         SCALE, SESSION_ID, 0, mac, GROUP, b"v0.1.0", N,
                         CAR_NAME.encode(), b"\x00" * 18)
    body = bytearray()
    step_ms = 1000 // RATE_HZ
    for i in range(N):
        body += struct.pack("<I", i * step_ms)
        for v in frame(i / RATE_HZ, base, hot_col, tau):
            iv = max(-32768, min(32767, round(v * SCALE)))
            body += struct.pack("<h", iv)
    path = os.path.join(folder, f"{name}_{SESSION_ID}.bin")
    with open(path, "wb") as f:
        f.write(header + body)
    return path, len(header) + len(body)


def main():
    folder = os.path.join(os.path.expanduser("~"), "SI_Tyre_Analyzer_Runs")
    os.makedirs(folder, exist_ok=True)
    print(f"run {SESSION_ID}  {N} frames @ {RATE_HZ}Hz  ({DURATION_S//60} min)")
    for name, (wp, base, hot, tau) in WHEELS.items():
        path, size = write_wheel(folder, name, wp, base, hot, tau)
        print(f"  {os.path.basename(path)}  {size} B")
    print(f"-> {folder}")


if __name__ == "__main__":
    main()
