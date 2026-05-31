"""A single tyre's heatmap (title + colored grid + min/max), painted with Qt."""

from __future__ import annotations

import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QFont, QPainter
from PySide6.QtWidgets import QLabel, QSizePolicy, QVBoxLayout, QWidget

from . import theme
from .colors import heat_rgb


class _HeatCanvas(QWidget):
    def __init__(self):
        super().__init__()
        self._grid: np.ndarray | None = None  # (rows, cols) deg C
        self.setMinimumSize(120, 160)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

    def setGrid(self, grid):
        self._grid = grid
        self.update()

    def paintEvent(self, _ev):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(theme.BG))
        g = self._grid
        if g is None or g.size == 0:
            return
        rows, cols = g.shape
        w, h = self.width(), self.height()
        lo, hi = float(np.nanmin(g)), float(np.nanmax(g))
        mid = rows // 2
        f = QFont()
        f.setPixelSize(max(8, min(w // cols // 2, 14)))
        f.setBold(True)
        p.setFont(f)
        for r in range(rows):
            y0 = r * h // rows
            y1 = (r + 1) * h // rows
            for c in range(cols):
                x0 = c * w // cols
                x1 = (c + 1) * w // cols
                rr, gg, bb = heat_rgb(float(g[r, c]), lo, hi)
                p.fillRect(x0, y0, x1 - x0, y1 - y0, QColor(rr, gg, bb))
                if r == mid:
                    p.setPen(QColor(theme.WHITE))
                    p.drawText(
                        x0,
                        y0,
                        x1 - x0,
                        y1 - y0,
                        Qt.AlignCenter,
                        str(round(float(g[r, c]))),
                    )


class TyreView(QWidget):
    """Title label + heat canvas + min/max line for one wheel."""

    def __init__(self, title: str):
        super().__init__()
        lay = QVBoxLayout(self)
        lay.setContentsMargins(6, 6, 6, 6)
        lay.setSpacing(4)
        self._title = QLabel(title)
        self._title.setAlignment(Qt.AlignCenter)
        self._title.setStyleSheet("font-weight:600;")
        self._canvas = _HeatCanvas()
        self._stats = QLabel("—")
        self._stats.setAlignment(Qt.AlignCenter)
        self._stats.setStyleSheet(f"color:{theme.GRID_TEXT};font-weight:600;")
        lay.addWidget(self._title)
        lay.addWidget(self._canvas, 1)
        lay.addWidget(self._stats)

    def setTitle(self, t: str):
        self._title.setText(t)

    def setGrid(self, grid):
        if grid is None:
            self._canvas.setGrid(None)
            self._stats.setText("—")
            return
        self._canvas.setGrid(grid)
        self._stats.setText(
            f"min {float(np.nanmin(grid)):.1f}°  ·  max {float(np.nanmax(grid)):.1f}°"
        )

    def clear(self):
        self.setGrid(None)
