"""SI Tyre Analyzer — desktop app entry point (PySide6)."""

from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtCore import QTimer
from PySide6.QtGui import QIcon
from PySide6.QtSvgWidgets import QSvgWidget
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QListWidget,
    QMessageBox,
    QPushButton,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

from . import update
from .net import Worker
from .pages.analysis import AnalysisPage
from .pages.library import LibraryPage
from .pages.live import LivePage
from .pages.viewer import ViewerPage
from .theme import DARK_QSS

ASSETS = Path(__file__).parent / "assets"


class MainWindow(QWidget):
    """Top-level window: header, nav, and the stacked pages."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("SI Tyre Analyzer")
        self.setWindowIcon(QIcon(str(ASSETS / "si_tyre_mark.svg")))
        self.resize(960, 680)

        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        header = QWidget()
        header.setObjectName("header")
        header.setFixedHeight(66)
        hb = QHBoxLayout(header)
        hb.setContentsMargins(16, 10, 16, 10)
        logo = QSvgWidget(str(ASSETS / "si_tyre_logo.svg"))
        logo.setFixedSize(172, 46)
        hb.addWidget(logo)
        hb.addStretch(1)
        self._update_btn = QPushButton("Check for updates")
        self._update_btn.setObjectName("ghost")
        self._update_btn.clicked.connect(lambda: self._check_updates(manual=True))
        hb.addWidget(self._update_btn)
        root.addWidget(header)

        body = QHBoxLayout()
        body.setContentsMargins(0, 0, 0, 0)
        body.setSpacing(0)
        root.addLayout(body, 1)

        self._nav = QListWidget()
        self._nav.setObjectName("nav")
        self._nav.setFixedWidth(170)
        for name in ["Sessions", "Live", "Viewer", "Analysis"]:
            self._nav.addItem(name)
        body.addWidget(self._nav)

        self._stack = QStackedWidget()
        self._live = LivePage()
        self._library = LibraryPage()
        self._viewer = ViewerPage()
        self._analysis = AnalysisPage()
        for pg in (self._library, self._live, self._viewer, self._analysis):
            self._stack.addWidget(pg)
        body.addWidget(self._stack, 1)

        self._nav.currentRowChanged.connect(self._stack.setCurrentIndex)
        self._nav.setCurrentRow(0)
        self._library.openRun.connect(self._open_run)

        QTimer.singleShot(1500, lambda: self._check_updates(manual=False))

    def _open_run(self, run: dict):
        self._viewer.load_run(run)
        self._analysis.load_run(run)
        self._nav.setCurrentRow(2)

    def _check_updates(self, manual: bool):
        self._update_btn.setEnabled(False)
        self._update_btn.setText("Checking…")
        self._chk = Worker(update.check)
        self._chk.done.connect(lambda rel: self._on_checked(rel, manual))
        self._chk.failed.connect(lambda err: self._on_check_failed(err, manual))
        self._chk.start()

    def _reset_update_btn(self):
        self._update_btn.setEnabled(True)
        self._update_btn.setText("Check for updates")

    def _on_checked(self, rel, manual: bool):
        self._reset_update_btn()
        if rel is None:
            if manual:
                QMessageBox.information(
                    self,
                    "Up to date",
                    f"You're on the latest version ({update.current_version()}).",
                )
            return
        if (
            QMessageBox.question(
                self,
                "Update available",
                f"Version {rel.version} is available "
                f"(you have {update.current_version()}).\n\n"
                "Install it now? The app will restart.",
            )
            != QMessageBox.Yes
        ):
            return
        self._update_btn.setEnabled(False)
        self._update_btn.setText("Updating…")
        self._upd = Worker(lambda: update.apply(rel))
        self._upd.done.connect(lambda _: self._on_updated())
        self._upd.failed.connect(self._on_update_failed)
        self._upd.start()

    def _on_check_failed(self, err: str, manual: bool):
        self._reset_update_btn()
        if manual:
            QMessageBox.warning(
                self, "Update check failed", f"Could not check for updates.\n\n{err}"
            )

    def _on_updated(self):
        QMessageBox.information(
            self,
            "Update installed",
            "The update was installed. The app will now restart.",
        )
        update.relaunch()
        QApplication.quit()

    def _on_update_failed(self, err: str):
        self._reset_update_btn()
        QMessageBox.warning(self, "Update failed", f"The update failed.\n\n{err}")

    def closeEvent(self, ev):
        self._live.close()
        super().closeEvent(ev)


def main():
    app = QApplication(sys.argv)
    app.setWindowIcon(QIcon(str(ASSETS / "si_tyre_mark.svg")))
    app.setStyleSheet(DARK_QSS)
    win = MainWindow()
    win.show()
    run_loop = app.exec
    sys.exit(run_loop())


if __name__ == "__main__":
    main()
