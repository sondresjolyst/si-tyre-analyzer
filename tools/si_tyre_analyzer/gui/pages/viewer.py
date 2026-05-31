"""Heatmap viewer: open a run and play/scrub the 4-wheel heatmaps."""

from __future__ import annotations

from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import (
    QComboBox,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSlider,
    QVBoxLayout,
    QWidget,
)

from ...constants import WHEELS
from ..heatmap_widget import TyreView
from ..widgets import RunSelector


class ViewerPage(QWidget):
    """Replay a run: scrub and play the four heatmaps with a speed control."""

    def __init__(self):
        super().__init__()
        self._run = {}  # current {wheel: SessionData}
        self._n = 0
        self._base_ms = 500.0
        self._speed = 1

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._advance)

        root = QVBoxLayout(self)
        top_bar = QHBoxLayout()
        self._sel = RunSelector()
        self._sel.runChanged.connect(self._on_run)
        top_bar.addWidget(self._sel, 1)
        root.addLayout(top_bar)

        grid = QGridLayout()
        self._tyres = {}
        for i, w in enumerate(WHEELS):
            tv = TyreView(w)
            self._tyres[w] = tv
            grid.addWidget(tv, i // 2, i % 2)
        root.addLayout(grid, 1)

        sl = QHBoxLayout()
        self._play = QPushButton("Play")
        self._play.setFixedWidth(72)
        self._play.clicked.connect(self._toggle_play)
        sl.addWidget(self._play)
        self._spd = QComboBox()
        self._spd.addItems(["1x", "2x", "4x"])
        self._spd.currentIndexChanged.connect(self._speed_changed)
        sl.addWidget(self._spd)
        self._slider = QSlider(Qt.Horizontal)
        self._slider.valueChanged.connect(self._show_frame)
        sl.addWidget(self._slider, 1)
        self._tlabel = QLabel("t = 0.0 s")
        sl.addWidget(self._tlabel)
        root.addLayout(sl)

    # ---- loading ----
    def load_run(self, run: dict):
        self._sel.load_run(run)

    def _on_run(self, run: dict):
        self._run = run
        self._setup_frames()

    def _setup_frames(self):
        if not self._run:
            return
        self._n = min(len(s.grids) for s in self._run.values())
        rate = next(iter(self._run.values())).sample_rate_hz or 1
        self._base_ms = 1000.0 / rate
        self._slider.setRange(0, max(0, self._n - 1))
        self._slider.setValue(0)
        self._show_frame(0)

    # ---- playback ----
    def _toggle_play(self):
        if self._timer.isActive():
            self._timer.stop()
            self._play.setText("Play")
        elif self._n:
            self._timer.start(int(self._base_ms / self._speed))
            self._play.setText("Pause")

    def _speed_changed(self, idx):
        self._speed = [1, 2, 4][idx]
        if self._timer.isActive():
            self._timer.start(int(self._base_ms / self._speed))

    def _advance(self):
        if self._n:
            self._slider.setValue((self._slider.value() + 1) % self._n)

    def _show_frame(self, i: int):
        if not self._run or i >= self._n:
            return
        for w, tv in self._tyres.items():
            s = self._run.get(w)
            tv.setGrid(s.grids[i] if s is not None else None)
        t = next(iter(self._run.values())).t_offsets_ms[i] / 1000.0
        self._tlabel.setText(f"t = {t:.1f} s")
