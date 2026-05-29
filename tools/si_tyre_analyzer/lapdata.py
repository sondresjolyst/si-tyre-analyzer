"""Parse a lap-timing CSV (Lap, Pos, Lap Time, ... , Speed) into lap markers."""

from __future__ import annotations

import csv
from dataclasses import dataclass


@dataclass
class Lap:
    lap: int
    time_s: float
    start_s: float  # cumulative time at lap start (race t=0 at green flag)
    pos: int | None = None


def _mmss(s: str) -> float | None:
    s = s.strip()
    try:
        if ":" in s:
            m, rest = s.split(":")
            return int(m) * 60 + float(rest)
        return float(s)
    except ValueError:
        return None


def parse_laps(path: str) -> list[Lap]:
    """Return laps with cumulative start times. Empty list if not parseable."""
    laps: list[Lap] = []
    cum = 0.0
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            t = _mmss(row.get("Lap Time", ""))
            if t is None:
                continue
            try:
                num = int(row.get("Lap", len(laps) + 1))
            except ValueError:
                num = len(laps) + 1
            pos = None
            try:
                pos = int(row.get("Pos", ""))
            except (ValueError, TypeError):
                pass
            laps.append(Lap(lap=num, time_s=t, start_s=cum, pos=pos))
            cum += t
    return laps
