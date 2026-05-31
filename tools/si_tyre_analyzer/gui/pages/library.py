"""Session library: list/download runs from a device over WiFi + local library."""

from __future__ import annotations

import os

from PySide6.QtCore import Qt, QUrl, Signal
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (QFileDialog, QHBoxLayout, QLabel, QLineEdit,
                               QListWidget, QListWidgetItem, QMessageBox,
                               QPushButton, QVBoxLayout, QWidget)

from ...constants import DEFAULT_HOST
from .. import firmware, net, prefs, theme
from ...fetch import download, download_all, list_sessions
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
        self._host = QLineEdit(DEFAULT_HOST)
        self._host.setMaximumWidth(180)
        dev.addWidget(self._host)
        b_list = QPushButton("List sessions")
        b_list.clicked.connect(self._list_device)
        dev.addWidget(b_list)
        b_dl1 = QPushButton("Download selected")
        b_dl1.clicked.connect(self._download_selected)
        dev.addWidget(b_dl1)
        b_dl = QPushButton("Download all")
        b_dl.clicked.connect(self._download_all)
        dev.addWidget(b_dl)
        b_cfg = QPushButton("Configure…")
        b_cfg.clicked.connect(self._open_config)
        dev.addWidget(b_cfg)
        self._b_fw = QPushButton("Update firmware…")
        self._b_fw.clicked.connect(self._update_fw)
        dev.addWidget(self._b_fw)
        dev.addStretch(1)
        root.addLayout(dev)
        self._devlist = QListWidget()
        self._devlist.itemDoubleClicked.connect(self._download_item)
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
        self._status.setStyleSheet(f"color:{theme.MUTED};")
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

    def _open_config(self):
        host = self._host.text().strip() or DEFAULT_HOST
        QDesktopServices.openUrl(QUrl(f"http://{host}/"))

    # ---- firmware ----
    def _update_fw(self):
        host = self._host.text().strip() or DEFAULT_HOST
        self._b_fw.setEnabled(False)
        self._b_fw.setText("Checking…")
        self._status.setText("Checking for firmware…")
        self._fwk = net.Worker(lambda: firmware.check(host))
        self._fwk.done.connect(lambda rel: self._fw_checked(host, rel))
        self._fwk.failed.connect(self._fw_failed)
        self._fwk.start()

    def _reset_fw_btn(self):
        self._b_fw.setEnabled(True)
        self._b_fw.setText("Update firmware…")

    def _fw_checked(self, host, rel):
        if rel is None:
            self._reset_fw_btn()
            self._status.setText("Firmware up to date")
            QMessageBox.information(
                self, "Up to date",
                f"This device is on the latest firmware "
                f"({firmware.device_version(host)}).")
            return
        cur = firmware.device_version(host)
        if QMessageBox.question(
                self, "Update available",
                f"Firmware {rel.version} is available (device has {cur}).\n\n"
                "Update the car? The wheel units will flash over the air.") \
                != QMessageBox.Yes:
            self._reset_fw_btn()
            self._status.setText("")
            return
        self._b_fw.setText("Updating…")
        self._status.setText("Pushing firmware to wheels…")
        self._fwc = net.Worker(lambda: firmware.cascade(host, rel))
        self._fwc.done.connect(lambda _: self._fw_cascaded(host, rel))
        self._fwc.failed.connect(self._fw_failed)
        self._fwc.start()

    def _fw_cascaded(self, host, rel):
        self._status.setText("Wheels updating over the air")
        if QMessageBox.question(
                self, "Update the master?",
                "The wheel units are now flashing.\n\n"
                "Also flash this master device? It will reboot and "
                "disconnect from Wi-Fi.") != QMessageBox.Yes:
            self._reset_fw_btn()
            return
        self._b_fw.setText("Flashing master…")
        self._fwm = net.Worker(lambda: firmware.flash_master(host, rel))
        self._fwm.done.connect(lambda _: self._fw_master_done())
        self._fwm.failed.connect(self._fw_failed)
        self._fwm.start()

    def _fw_master_done(self):
        self._reset_fw_btn()
        self._status.setText("Master rebooting")
        QMessageBox.information(
            self, "Master updating",
            "The master is flashing and will reboot. Reconnect to its "
            "Wi-Fi once it returns.")

    def _fw_failed(self, msg):
        self._reset_fw_btn()
        self._status.setText(f"Error: {msg}")
        QMessageBox.warning(self, "Firmware update failed", msg)

    def _list_device(self):
        host = self._host.text().strip()
        self._run_worker(lambda: list_sessions(host), self._show_device_list)

    def _show_device_list(self, sessions):
        self._devlist.clear()
        for s in sessions:
            it = QListWidgetItem(
                f"{s.get('wheel','?')}  {s.get('name','')}  "
                f"{s.get('records','?')} recs")
            it.setData(Qt.UserRole, s.get("name", ""))
            self._devlist.addItem(it)
        self._status.setText(f"{len(sessions)} session(s) on device")

    def _download_selected(self):
        self._download_item(self._devlist.currentItem())

    def _download_item(self, it):
        if not it:
            self._status.setText("Select a session first")
            return
        name = it.data(Qt.UserRole)
        if not name:
            return
        host = self._host.text().strip()
        dest = self._dir.text().strip()
        os.makedirs(dest, exist_ok=True)
        self._run_worker(lambda: download(host, name, dest),
                         lambda _p: self._after_download([_p]))

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
