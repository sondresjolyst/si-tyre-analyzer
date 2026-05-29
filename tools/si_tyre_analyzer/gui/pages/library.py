"""Session library: list/download runs from a device over WiFi + local library."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QFileDialog, QHBoxLayout, QLabel, QLineEdit,
                               QListWidget, QListWidgetItem, QPushButton,
                               QVBoxLayout, QWidget)

from .. import net, prefs
from ...fetch import download_all, list_sessions
from ..runs import DEFAULT_DIR, load_runs, run_label


class LibraryPage(QWidget):
    openRun = Signal(dict)  # {wheel: SessionData}

    def __init__(self):
        super().__init__()
        self._worker = None
        os.makedirs(DEFAULT_DIR, exist_ok=True)

        root = QVBoxLayout(self)

        # --- device fetch ---
        root.addWidget(QLabel("<b>Device</b>"))
        dev = QHBoxLayout()
        dev.addWidget(QLabel("Host:"))
        self._host = QLineEdit("192.168.4.1")
        self._host.setMaximumWidth(180)
        dev.addWidget(self._host)
        b_list = QPushButton("List sessions")
        b_list.clicked.connect(self._list_device)
        dev.addWidget(b_list)
        b_dl = QPushButton("Download all")
        b_dl.clicked.connect(self._download_all)
        dev.addWidget(b_dl)
        dev.addStretch(1)
        root.addLayout(dev)
        self._devlist = QListWidget()
        root.addWidget(self._devlist)

        # --- local library ---
        root.addWidget(QLabel("<b>Library</b>"))
        loc = QHBoxLayout()
        self._dir = QLineEdit(prefs.last_dir(DEFAULT_DIR))
        loc.addWidget(self._dir, 1)
        b_browse = QPushButton("Browse…")
        b_browse.clicked.connect(self._browse)
        loc.addWidget(b_browse)
        b_refresh = QPushButton("Refresh")
        b_refresh.clicked.connect(self._refresh)
        loc.addWidget(b_refresh)
        b_open = QPushButton("Open in viewer")
        b_open.clicked.connect(self._open)
        loc.addWidget(b_open)
        root.addLayout(loc)
        self._runlist = QListWidget()
        root.addWidget(self._runlist, 1)

        self._status = QLabel("")
        self._status.setStyleSheet("color:#9ca3af;")
        root.addWidget(self._status)

        self._runs = {}
        self._refresh()

    # ---- device ----
    def _run_worker(self, fn, on_done):
        host = self._host.text().strip()
        self._status.setText("Working…")
        self._worker = net.Worker(fn)
        self._worker.done.connect(on_done)
        self._worker.failed.connect(lambda m: self._status.setText(f"Error: {m}"))
        self._worker.start()

    def _list_device(self):
        host = self._host.text().strip()
        self._run_worker(lambda: list_sessions(host), self._show_device_list)

    def _show_device_list(self, sessions):
        self._devlist.clear()
        for s in sessions:
            self._devlist.addItem(
                f"{s.get('wheel','?')}  {s.get('name','')}  "
                f"{s.get('records','?')} recs")
        self._status.setText(f"{len(sessions)} session(s) on device")

    def _download_all(self):
        host = self._host.text().strip()
        dest = self._dir.text().strip()
        os.makedirs(dest, exist_ok=True)
        self._run_worker(lambda: download_all(host, dest), self._after_download)

    def _after_download(self, paths):
        self._status.setText(f"Downloaded {len(paths)} file(s)")
        self._refresh()

    # ---- local ----
    def _browse(self):
        d = QFileDialog.getExistingDirectory(self, "Library folder",
                                             self._dir.text())
        if d:
            self._dir.setText(d)
            self._refresh()

    def _refresh(self):
        prefs.set_last_dir(self._dir.text().strip())
        self._runs = load_runs(self._dir.text().strip())
        self._runlist.clear()
        for sid in sorted(self._runs):
            it = QListWidgetItem(run_label(self._runs[sid]))
            it.setData(Qt.UserRole, sid)
            self._runlist.addItem(it)

    def _open(self):
        it = self._runlist.currentItem()
        if not it:
            return
        sid = it.data(Qt.UserRole)
        run = self._runs.get(sid)
        if run:
            self.openRun.emit(run)
