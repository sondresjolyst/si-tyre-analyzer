"""Persisted UI preferences (last folder, target window) via QSettings."""

from __future__ import annotations

from PySide6.QtCore import QSettings


def _s() -> QSettings:
    return QSettings("SI Tyre Analyzer", "SI Tyre Analyzer")


def last_dir(default: str) -> str:
    return _s().value("last_dir", default)


def set_last_dir(path: str) -> None:
    _s().setValue("last_dir", path)


def window(default_lo: float, default_hi: float) -> tuple[float, float]:
    s = _s()
    return (float(s.value("win_lo", default_lo)),
            float(s.value("win_hi", default_hi)))


def set_window(lo: float, hi: float) -> None:
    s = _s()
    s.setValue("win_lo", lo)
    s.setValue("win_hi", hi)
