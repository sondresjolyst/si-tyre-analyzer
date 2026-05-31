"""Per-run metadata stored as a JSON sidecar next to the .bin files."""

from __future__ import annotations

import json
import os

# Display label keyed by storage field.
FIELDS = {
    "date": "Date",
    "track": "Track",
    "driver": "Driver",
    "compound": "Compound",
    "ambient": "Ambient °C",
    "p_fl": "Pressure FL",
    "p_fr": "Pressure FR",
    "p_rl": "Pressure RL",
    "p_rr": "Pressure RR",
    "notes": "Notes",
}


def _path(folder: str, session_id: int) -> str:
    return os.path.join(folder, f"session_{session_id}.json")


def load(folder: str, session_id: int) -> dict:
    try:
        with open(_path(folder, session_id), encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def save(folder: str, session_id: int, data: dict) -> None:
    clean = {k: v for k, v in data.items() if k in FIELDS and str(v).strip()}
    path = _path(folder, session_id)
    if not clean:
        if os.path.exists(path):
            os.remove(path)
        return
    with open(path, "w", encoding="utf-8") as f:
        json.dump(clean, f, indent=2)


def summary(data: dict) -> str:
    """One-line summary for the library list, '' if empty."""
    bits = [data[k] for k in ("track", "driver", "compound") if data.get(k)]
    return "  ·  ".join(str(b) for b in bits)
