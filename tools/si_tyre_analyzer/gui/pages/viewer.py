"""Heatmap viewer: open a run and play/scrub the 4-wheel heatmaps."""

from __future__ import annotations

from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import (QComboBox, QFileDialog, QGridLayout, QHBoxLayout,
                               QLabel, QPushButton, QSlider, QVBoxLayout,
                               QWidget)

from .. import prefs
from ..heatmap_widget import TyreView
from ..runs import DEFAULT_DIR, load_runs, run_label

WHEELS = ["FL", "FR", "RL", "RR"]


class ViewerPage(QWidget):
    def __init__(self):
        super().__init__()
        self._runs = {}          # sid -> {wheel: SessionData}
        self._run = {}           # current {wheel: SessionData}
        self._n = 0
        self._base_ms = 500.0
        self._speed = 1

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._advance)

        root = QVBoxLayout(self)
        bar = QHBoxLayout()
        b_open = QPushButton("Open folder…")
        b_open.clicked.connect(self._open_folder)
        bar.addWidget(b_open)
        self._runsel = QComboBox()
        self._runsel.currentIndexChanged.connect(self._run_selected)
        bar.addWidget(self._runsel, 1)
        root.addLayout(bar)

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
    def _open_folder(self):
        d = QFileDialog.getExistingDirectory(self, "Open runs folder",
                                             prefs.last_dir(DEFAULT_DIR))
        if not d:
            return
        prefs.set_last_dir(d)
        self._runs = load_runs(d)
        first = sorted(self._runs)[0] if self._runs else None
        self._refresh_combo(first)

    def load_run(self, run: dict):
        """Load a run dict {wheel: SessionData} directly (from the library)."""
        if not run:
            return
        sid = next(iter(run.values())).session_id
        self._runs[sid] = run
        self._refresh_combo(sid)

    def _refresh_combo(self, select_sid):
        self._runsel.blockSignals(True)
        self._runsel.clear()
        for sid in sorted(self._runs):
            self._runsel.addItem(run_label(self._runs[sid]), sid)
        if select_sid is not None:
            self._runsel.setCurrentIndex(list(sorted(self._runs)).index(select_sid))
        self._runsel.blockSignals(False)
        self._run = self._runs.get(self._runsel.currentData(), {})
        self._setup_frames()

    def _run_selected(self, _idx):
        sid = self._runsel.currentData()
        if sid is None:
            return
        self._run = self._runs.get(sid, {})
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
