"""A single tyre's heatmap (title + colored grid + min/max), painted with Qt."""

from __future__ import annotations

import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPainterPath
from PySide6.QtWidgets import QLabel, QSizePolicy, QVBoxLayout, QWidget

from ..constants import OPT_HI_DEFAULT, OPT_LO_DEFAULT
from . import theme
from .colors import heat_rgb, scale_hi, scale_lo

# Tyre patch aspect (width : height). < 1 = portrait, wheel-like. Shared so the
# Analysis Balance heatmaps match the Live/Viewer patch shape.
PATCH_ASPECT = 0.78


def _mono(px: int, bold: bool = True) -> QFont:
    f = QFont()
    f.setFamilies(theme.MONO_FAMILIES)
    f.setStyleHint(QFont.Monospace)
    f.setPixelSize(px)
    f.setBold(bold)
    return f


class _HeatCanvas(QWidget):
    def __init__(self):
        super().__init__()
        self._grid: np.ndarray | None = None  # (rows, cols) deg C
        self._opt_lo: float = OPT_LO_DEFAULT
        self._opt_hi: float = OPT_HI_DEFAULT
        self._align = False
        self.setMinimumSize(120, 160)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

    def setGrid(self, grid, opt_lo: float | None = None, opt_hi: float | None = None):
        self._grid = grid
        if opt_lo is not None and opt_hi is not None:
            self._opt_lo, self._opt_hi = opt_lo, opt_hi
        self.update()

    def setAlign(self, on: bool):
        self._align = on
        self.update()

    def paintEvent(self, _ev):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(theme.BG_DEEP))
        g = self._grid
        if g is None or g.size == 0:
            return
        rows, cols = g.shape
        opt_lo, opt_hi = self._opt_lo, self._opt_hi
        mid = rows // 2
        # Render the patch at a tyre-like aspect (a touch taller than wide),
        # centred with margins, rather than stretching to the whole tile.
        ox, oy, w, h = self._patch_rect()
        p.setRenderHint(QPainter.Antialiasing)
        radius = min(w, h) * 0.16
        clip = QPainterPath()
        clip.addRoundedRect(ox, oy, w, h, radius, radius)
        p.setClipPath(clip)
        p.setFont(_mono(max(8, min(w // cols // 2, 14))))
        for r in range(rows):
            y0 = oy + r * h // rows
            y1 = oy + (r + 1) * h // rows
            for c in range(cols):
                x0 = ox + c * w // cols
                x1 = ox + (c + 1) * w // cols
                rr, gg, bb = heat_rgb(float(g[r, c]), opt_lo, opt_hi)
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
        if self._align:
            self._draw_guides(p, cols, (ox, oy, w, h))

    def _patch_rect(self) -> tuple[int, int, int, int]:
        aw, ah = self.width(), self.height()
        w = min(aw, round(ah * PATCH_ASPECT))
        h = min(ah, round(w / PATCH_ASPECT))
        return (aw - w) // 2, (ah - h) // 2, w, h

    def _draw_guides(self, p, cols, rect):
        # Columns run across the tyre tread (inner -> outer); the centre row
        # line marks the around-the-tyre middle.
        ox, oy, w, h = rect
        p.setPen(QColor(255, 255, 255, 150))
        p.drawLine(ox, oy + h // 2, ox + w, oy + h // 2)
        for c in (cols // 3, 2 * cols // 3):
            x = ox + c * w // cols
            p.drawLine(x, oy, x, oy + h)
        p.setFont(_mono(10))
        bands = [
            (0, cols // 3, "INNER"),
            (cols // 3, 2 * cols // 3, "MID"),
            (2 * cols // 3, cols, "OUTER"),
        ]
        for a, b, txt in bands:
            x0 = ox + a * w // cols
            x1 = ox + b * w // cols
            p.setPen(QColor(0, 0, 0, 140))
            p.drawText(x0 + 1, oy + 3, x1 - x0, 16, Qt.AlignHCenter | Qt.AlignTop, txt)
            p.setPen(QColor(255, 255, 255, 230))
            p.drawText(x0, oy + 2, x1 - x0, 16, Qt.AlignHCenter | Qt.AlignTop, txt)


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
        self._stats.setStyleSheet(
            f"color:{theme.GRID_TEXT}; font-weight:600; font-family:{theme.MONO};"
        )
        lay.addWidget(self._title)
        lay.addWidget(self._canvas, 1)
        lay.addWidget(self._stats)

    def setTitle(self, t: str):
        self._title.setText(t)

    def setAlign(self, on: bool):
        self._canvas.setAlign(on)

    def setGrid(self, grid, opt_lo: float | None = None, opt_hi: float | None = None):
        if grid is None:
            self._canvas.setGrid(None)
            self._stats.setText("—")
            return
        self._canvas.setGrid(grid, opt_lo, opt_hi)
        self._stats.setText(
            f"min {float(np.nanmin(grid)):4.1f}°   "
            f"avg {float(np.nanmean(grid)):4.1f}°   "
            f"max {float(np.nanmax(grid)):4.1f}°"
        )

    def clear(self):
        self.setGrid(None)


class ScaleLegend(QWidget):
    """Horizontal heat gradient bar with lo/mid/hi °C labels — the shared
    temperature scale all four tyres are coloured against."""

    def __init__(self):
        super().__init__()
        self._opt_lo: float = OPT_LO_DEFAULT
        self._opt_hi: float = OPT_HI_DEFAULT
        self.setFixedHeight(30)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)

    def setRange(self, opt_lo: float, opt_hi: float):
        self._opt_lo, self._opt_hi = opt_lo, opt_hi
        self.update()

    def paintEvent(self, _ev):
        p = QPainter(self)
        w, h = self.width(), self.height()
        bar_h = 11
        lo = scale_lo(self._opt_lo, self._opt_hi)
        hi = scale_hi(self._opt_lo, self._opt_hi)
        grad = QLinearGradient(0, 0, w, 0)
        for i in range(0, 257, 16):
            f = min(1.0, i / 255.0)
            rr, gg, bb = heat_rgb(lo + (hi - lo) * f, self._opt_lo, self._opt_hi)
            grad.setColorAt(f, QColor(rr, gg, bb))
        p.fillRect(0, 0, w, bar_h, grad)
        p.setPen(QColor(theme.BORDER))
        p.drawRect(0, 0, w - 1, bar_h)
        p.setFont(_mono(11, bold=False))
        p.setPen(QColor(theme.TEXT_DIM))
        ty, th = bar_h + 1, h - bar_h - 1
        mid = (lo + hi) / 2
        p.drawText(0, ty, w, th, Qt.AlignLeft | Qt.AlignVCenter, f"{lo:.0f}°")
        p.drawText(0, ty, w, th, Qt.AlignHCenter | Qt.AlignVCenter, f"{mid:.0f}°")
        p.drawText(0, ty, w, th, Qt.AlignRight | Qt.AlignVCenter, f"{hi:.0f}°")
