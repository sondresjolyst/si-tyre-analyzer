"""Group local .bin session files into runs (by session_id)."""

from __future__ import annotations

import glob
import os

from PySide6.QtCore import QStandardPaths

from ..logreader import SessionData, read_session


def _default_dir() -> str:
    docs = QStandardPaths.writableLocation(
        QStandardPaths.StandardLocation.DocumentsLocation
    ) or os.path.expanduser("~")
    return os.path.normpath(os.path.join(docs, "SI Tyre Analyzer"))


DEFAULT_DIR = _default_dir()


def scan_runs(
    folder: str,
) -> tuple[dict[int, dict[str, SessionData]], list[tuple[str, str]]]:
    """Like load_runs, but also return [(filename, reason)] for files skipped."""
    runs: dict[int, dict[str, SessionData]] = {}
    skipped: list[tuple[str, str]] = []
    for path in sorted(glob.glob(os.path.join(folder, "*.bin"))):
        try:
            s = read_session(path)
        except Exception as e:  # noqa: BLE001  (any malformed file -> skip + report)
            skipped.append((os.path.basename(path), str(e)))
            continue
        runs.setdefault(s.session_id, {})[s.wheel] = s
    return runs, skipped


def load_runs(folder: str) -> dict[int, dict[str, SessionData]]:
    """Return {session_id: {wheel: SessionData}} for every .bin in folder."""
    return scan_runs(folder)[0]


def run_label(run: dict) -> str:
    """Human label for a run dict: '<car> — #<id> (FL,FR,...)'."""
    if not run:
        return ""
    s = next(iter(run.values()))
    car = getattr(s, "car_name", "") or "Run"
    wheels = ",".join(sorted(run))
    return f"{car} — #{s.session_id}  ({wheels})"
