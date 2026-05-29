"""Cross-language format contract test.

Builds a .bin matching src/storage/LogFormat.h, then verifies the logreader
parses it back correctly. Guards against drift between the C struct and the
Python parser. Runnable with pytest or directly: python tools/tests/test_logreader.py
"""

import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from si_tyre_analyzer.logreader import (HEADER_SIZE, LOG_MAGIC, LOG_VERSION,
                                        read_session)

FIXTURE = os.path.join(os.path.dirname(__file__), "fixtures", "sample_session.bin")

COLS, ROWS = 6, 3
RATE = 4
SCALE = 100
SESSION_ID = 42
GROUP_ID = 0x1234
WHEEL = 1  # FR
CAR_NAME = "Volvo 242 Turbo"
N = 5


def build_fixture(path, finalised=True):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    cells = COLS * ROWS
    header = struct.pack(
        "<IHBBHBBIQ6sI16sI24s18s",
        LOG_MAGIC, LOG_VERSION, COLS, ROWS, RATE, WHEEL, SCALE,
        SESSION_ID, 0, b"\x01\x02\x03\x04\x05\x06", GROUP_ID,
        b"v0.1.0", N if finalised else 0, CAR_NAME.encode(), b"\x00" * 18)
    assert len(header) == HEADER_SIZE

    body = bytearray()
    for i in range(N):
        body += struct.pack("<I", i * 250)  # t_offset 4 Hz
        for cell in range(cells):
            temp_c = 20.0 + i + cell
            body += struct.pack("<h", round(temp_c * SCALE))

    with open(path, "wb") as f:
        f.write(header + bytes(body))


def test_round_trip():
    build_fixture(FIXTURE, finalised=True)
    s = read_session(FIXTURE)
    assert s.cols == COLS and s.rows == ROWS
    assert s.wheel == "FR"
    assert s.session_id == SESSION_ID
    assert s.group_id == GROUP_ID
    assert s.car_name == CAR_NAME
    assert s.sample_rate_hz == RATE
    assert len(s.t_offsets_ms) == N
    assert s.grids.shape == (N, ROWS, COLS)
    # record i, cell 0 -> 20 + i
    assert abs(s.grids[2, 0, 0] - 22.0) < 0.01
    # t offsets
    assert int(s.t_offsets_ms[1]) == 250


def test_count_recovery_from_filesize():
    # record_count=0 (power loss) -> reader recovers N from file size.
    build_fixture(FIXTURE, finalised=False)
    s = read_session(FIXTURE)
    assert len(s.t_offsets_ms) == N


if __name__ == "__main__":
    test_round_trip()
    test_count_recovery_from_filesize()
    print("ALL PYTHON TESTS PASSED")
