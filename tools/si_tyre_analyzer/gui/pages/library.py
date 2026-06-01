"""Session library: list/download runs from a device over WiFi + local library."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys

from PySide6.QtCore import Qt, QUrl, Signal
from PySide6.QtGui import QDesktopServices
from PySide6.QtWidgets import (
    QFileDialog,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMenu,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

from ...constants import DEFAULT_HOST
from .. import firmware, meta, net, prefs, theme
from ...fetch import download, download_all, list_sessions
from ..metadialog import MetaDialog
from ..runs import DEFAULT_DIR, load_runs, run_label


def _reveal_in_file_manager(path: str) -> None:
    path = os.path.normpath(path)
    if sys.platform.startswith("win"):
        subprocess.Popen(["explorer", f"/select,{path}"])
    elif sys.platform == "darwin":
        subprocess.Popen(["open", "-R", path])
    else:
        subprocess.Popen(["xdg-open", os.path.dirname(path) or "."])


class LibraryPage(QWidget):
    """List/download runs from a device over Wi-Fi plus the local library."""

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
        b_info = QPushButton("Edit info…")
        b_info.clicked.connect(self._edit_info)
        loc.addWidget(b_info)
        root.addLayout(loc)
        self._runlist = QListWidget()
        self._runlist.itemDoubleClicked.connect(lambda _: self._open())
        self._runlist.setContextMenuPolicy(Qt.CustomContextMenu)
        self._runlist.customContextMenuRequested.connect(self._run_menu)
        root.addWidget(self._runlist, 1)

        self._status = QLabel("")
        self._status.setStyleSheet(f"color:{theme.MUTED};")
        root.addWidget(self._status)

        self._runs = {}
        self._refresh()

    # ---- device ----
    def _run_worker(self, fn, on_done):
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
                self,
                "Up to date",
                f"This device is on the latest firmware "
                f"({firmware.device_version(host)}).",
            )
            return
        cur = firmware.device_version(host)
        if (
            QMessageBox.question(
                self,
                "Update available",
                f"Firmware {rel.version} is available (device has {cur}).\n\n"
                "Update the car? The wheel units will flash over the air.",
            )
            != QMessageBox.Yes
        ):
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
        if (
            QMessageBox.question(
                self,
                "Update the master?",
                "The wheel units are now flashing.\n\n"
                "Also flash this master device? It will reboot and "
                "disconnect from Wi-Fi.",
            )
            != QMessageBox.Yes
        ):
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
            self,
            "Master updating",
            "The master is flashing and will reboot. Reconnect to its "
            "Wi-Fi once it returns.",
        )

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
                f"{s.get('wheel', '?')}  {s.get('name', '')}  "
                f"{s.get('records', '?')} recs"
            )
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
        self._run_worker(
            lambda: download(host, name, dest), lambda _p: self._after_download([_p])
        )

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
        d = QFileDialog.getExistingDirectory(self, "Library folder", self._dir.text())
        if d:
            self._dir.setText(d)
            self._refresh()

    def _refresh(self):
        prefs.set_last_dir(self._dir.text().strip())
        folder = self._dir.text().strip()
        self._runs = load_runs(folder)
        self._runlist.clear()
        for sid in sorted(self._runs):
            label = run_label(self._runs[sid])
            info = meta.summary(meta.load(folder, sid))
            if info:
                label += f"   —   {info}"
            it = QListWidgetItem(label)
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

    def _edit_info(self):
        it = self._runlist.currentItem()
        if not it:
            self._status.setText("Select a run first")
            return
        sid = it.data(Qt.UserRole)
        folder = self._dir.text().strip()
        dlg = MetaDialog(meta.load(folder, sid), self)
        if dlg.exec():
            meta.save(folder, sid, dlg.data())
            self._refresh()

    def _run_menu(self, pos):
        it = self._runlist.itemAt(pos)
        if not it:
            return
        self._runlist.setCurrentItem(it)
        menu = QMenu(self)
        menu.addAction("Open in viewer", self._open)
        menu.addAction("Edit info…", self._edit_info)
        menu.addAction("Reveal in folder", self._reveal)
        menu.addSeparator()
        menu.addAction("Delete run", self._delete_run)
        menu.exec(self._runlist.mapToGlobal(pos))

    def _run_paths(self, sid) -> list[str]:
        return [s.path for s in (self._runs.get(sid) or {}).values() if s.path]

    def _reveal(self):
        it = self._runlist.currentItem()
        if not it:
            return
        paths = self._run_paths(it.data(Qt.UserRole))
        target = paths[0] if paths else self._dir.text().strip()
        _reveal_in_file_manager(target)

    def _delete_run(self):
        it = self._runlist.currentItem()
        if not it:
            return
        sid = it.data(Qt.UserRole)
        paths = self._run_paths(sid)
        if (
            QMessageBox.question(
                self,
                "Delete run",
                f"Delete {len(paths)} file(s) for this run? This cannot be undone.",
            )
            != QMessageBox.Yes
        ):
            return
        for p in paths:
            try:
                os.remove(p)
            except OSError:
                pass
        meta.delete(self._dir.text().strip(), sid)
        self._refresh()

    def import_files(self, paths):
        """Copy dropped .bin files into the library folder."""
        dest = self._dir.text().strip()
        os.makedirs(dest, exist_ok=True)
        n = 0
        for p in paths:
            try:
                shutil.copy2(p, os.path.join(dest, os.path.basename(p)))
                n += 1
            except OSError:
                pass
        self._status.setText(f"Imported {n} file(s)")
        self._refresh()
