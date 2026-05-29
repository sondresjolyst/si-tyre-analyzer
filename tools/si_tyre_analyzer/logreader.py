"""Parse TyreTemp .bin session files.

The binary layout mirrors src/storage/LogFormat.h (little-endian, packed).
Keep this struct format and that header in lockstep.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

import numpy as np

LOG_MAGIC = 0x54595254  # "TYRT"
LOG_VERSION = 1

# magic, version, cols, rows, rate, wheel, scale, session_id,
# epoch_ms, mac[6], group_id, fw[16], record_count, car_name[24], reserved[18]
_HEADER_FMT = "<IHBBHBBIQ6sI16sI24s18s"
HEADER_SIZE = struct.calcsize(_HEADER_FMT)
assert HEADER_SIZE == 96, f"header must be 96 bytes, got {HEADER_SIZE}"

_WHEELS = {0: "FL", 1: "FR", 2: "RL", 3: "RR", 255: "NA"}


@dataclass
class SessionData:
    cols: int
    rows: int
    sample_rate_hz: int
    wheel: str
    temp_scale: int
    session_id: int
    epoch_ms: int
    mac: str
    group_id: int
    fw_version: str
    car_name: str
    t_offsets_ms: np.ndarray  # shape (N,)
    grids: np.ndarray         # shape (N, rows, cols), deg C

    @property
    def duration_s(self) -> float:
        return float(self.t_offsets_ms[-1]) / 1000.0 if len(self.t_offsets_ms) else 0.0


def read_session(path: str) -> SessionData:
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < HEADER_SIZE:
        raise ValueError("file too small for header")

    (magic, version, cols, rows, rate, wheel, scale, session_id, epoch_ms,
     mac, group_id, fw, record_count, car_b, _reserved) = struct.unpack(
        _HEADER_FMT, data[:HEADER_SIZE])

    if magic != LOG_MAGIC:
        raise ValueError(f"bad magic 0x{magic:08X}")
    if version != LOG_VERSION:
        raise ValueError(f"unsupported version {version}")

    car = car_b.split(b"\x00", 1)[0].decode("utf-8", "replace")
    cells = cols * rows
    stride = 4 + 2 * cells

    body = data[HEADER_SIZE:]
    # record_count may be 0 (not finalised / power loss) -> recover from size.
    n = record_count if record_count else len(body) // stride
    n = min(n, len(body) // stride)

    t_offsets = np.empty(n, dtype=np.uint32)
    grids = np.empty((n, rows, cols), dtype=np.float32)

    for i in range(n):
        rec = body[i * stride:(i + 1) * stride]
        t_offsets[i] = struct.unpack_from("<I", rec, 0)[0]
        temps = np.frombuffer(rec, dtype="<i2", count=cells, offset=4)
        grids[i] = temps.reshape(rows, cols).astype(np.float32) / scale

    fw_str = fw.split(b"\x00", 1)[0].decode("ascii", "replace")
    mac_str = ":".join(f"{b:02X}" for b in mac)

    return SessionData(
        cols=cols, rows=rows, sample_rate_hz=rate, wheel=_WHEELS.get(wheel, "NA"),
        temp_scale=scale, session_id=session_id, epoch_ms=epoch_ms, mac=mac_str,
        group_id=group_id, fw_version=fw_str, car_name=car,
        t_offsets_ms=t_offsets, grids=grids)
