"""Live dashboard page: poll the master's /api/live and show 4 wheels live."""

from __future__ import annotations

import numpy as np
from PySide6.QtWidgets import (
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QVBoxLayout,
    QWidget,
)

from ...constants import DEFAULT_HOST, WHEELS
from .. import theme
from ..heatmap_widget import TyreView
from ..icons import tool
from ..net import LivePoller


class LivePage(QWidget):
    """Poll the master's /api/live and show the four wheels updating live."""

    def __init__(self):
        super().__init__()
        self._poller: LivePoller | None = None

        root = QVBoxLayout(self)
        top_bar = QHBoxLayout()
        top_bar.addWidget(QLabel("Master host:"))
        self._host = QLineEdit(DEFAULT_HOST)
        self._host.setMaximumWidth(180)
        top_bar.addWidget(self._host)
        self._btn = tool("connect", "Connect", self._toggle)
        top_bar.addWidget(self._btn)
        self._status = QLabel("Not connected")
        self._status.setStyleSheet(f"color:{theme.MUTED};")
        top_bar.addWidget(self._status, 1)
        root.addLayout(top_bar)

        grid = QGridLayout()
        self._tyres = {}
        for i, w in enumerate(WHEELS):
            tv = TyreView(w)
            self._tyres[w] = tv
            grid.addWidget(tv, i // 2, i % 2)
        root.addLayout(grid, 1)

    def _toggle(self):
        if self._poller:
            self._stop()
        else:
            self._poller = LivePoller(self._host.text().strip())
            self._poller.data.connect(self._on_data)
            self._poller.error.connect(self._on_error)
            self._poller.start()
            self._btn.setToolTip("Disconnect")
            self._status.setText("Connecting…")

    def _stop(self):
        if self._poller:
            self._poller.stop()
            self._poller.wait(2000)
            self._poller = None
        self._btn.setToolTip("Connect")
        self._status.setStyleSheet(f"color:{theme.MUTED};")
        self._status.setText("Not connected")

    def _on_data(self, d: dict):
        cols, rows = int(d.get("cols", 0)), int(d.get("rows", 0))
        wheels = d.get("wheels", {})
        self._status.setStyleSheet(f"color:{theme.IN_WINDOW};")
        self._status.setText(f"Connected — {len(wheels)} wheel(s) live")
        for w, tv in self._tyres.items():
            wd = wheels.get(w)
            if wd and cols and rows:
                arr = np.array(wd["temps"], dtype=float).reshape(rows, cols)
                tv.setGrid(arr)
            else:
                tv.clear()

    def _on_error(self, msg: str):
        self._status.setText(f"Waiting… ({msg[:40]})")

    def closeEvent(self, ev):
        self._stop()
        super().closeEvent(ev)
