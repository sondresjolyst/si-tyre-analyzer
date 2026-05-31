"""Reusable GUI widgets."""

from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import QComboBox, QFileDialog, QHBoxLayout, QPushButton, QWidget

from . import prefs
from .runs import DEFAULT_DIR, load_runs, run_label


class RunSelector(QWidget):
    """Open-folder button + run dropdown. Emits runChanged({wheel: SessionData})."""

    runChanged = Signal(dict)

    def __init__(self):
        super().__init__()
        self._runs = {}
        lay = QHBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        b_open = QPushButton("Open folder…")
        b_open.clicked.connect(self._open_folder)
        lay.addWidget(b_open)
        self._combo = QComboBox()
        self._combo.currentIndexChanged.connect(self._selected)
        lay.addWidget(self._combo, 1)

    def current(self) -> dict:
        return self._runs.get(self._combo.currentData(), {})

    def load_run(self, run: dict):
        if not run:
            return
        sid = next(iter(run.values())).session_id
        self._runs[sid] = run
        self._rebuild(sid)

    def _open_folder(self):
        d = QFileDialog.getExistingDirectory(
            self, "Open runs folder", prefs.last_dir(DEFAULT_DIR)
        )
        if not d:
            return
        prefs.set_last_dir(d)
        self._runs = load_runs(d)
        self._rebuild(sorted(self._runs)[0] if self._runs else None)

    def _rebuild(self, select_sid):
        self._combo.blockSignals(True)
        self._combo.clear()
        for sid in sorted(self._runs):
            self._combo.addItem(run_label(self._runs[sid]), sid)
        if select_sid is not None:
            self._combo.setCurrentIndex(list(sorted(self._runs)).index(select_sid))
        self._combo.blockSignals(False)
        self.runChanged.emit(self.current())

    def _selected(self, _idx):
        self.runChanged.emit(self.current())
