"""Shared toolbar icons: monochrome SVGs styled for the dark theme."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import QSize
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QToolButton

ICONS = Path(__file__).resolve().parent / "assets" / "icons"


def icon(name: str) -> QIcon:
    return QIcon(str(ICONS / f"{name}.svg"))


def tool(name: str, tip: str, slot=None, size: int = 18) -> QToolButton:
    btn = QToolButton()
    btn.setIcon(icon(name))
    btn.setIconSize(QSize(size, size))
    btn.setToolTip(tip)
    if slot is not None:
        btn.clicked.connect(slot)
    return btn
