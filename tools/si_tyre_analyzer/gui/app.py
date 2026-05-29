"""SI Tyre Analyzer — desktop app entry point (PySide6)."""

from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtGui import QIcon
from PySide6.QtSvgWidgets import QSvgWidget
from PySide6.QtWidgets import (QApplication, QHBoxLayout, QListWidget,
                               QStackedWidget, QVBoxLayout, QWidget)

from .pages.analysis import AnalysisPage
from .pages.library import LibraryPage
from .pages.live import LivePage
from .pages.viewer import ViewerPage

DARK_QSS = """
QWidget { background:#111827; color:#f3f4f6; font-size:13px; }
QLineEdit, QComboBox, QListWidget, QTableWidget {
  background:#1f2937; border:1px solid #374151; border-radius:6px; padding:4px; }
QPushButton { background:#2563eb; color:#fff; border:0; border-radius:6px;
  padding:6px 12px; font-weight:600; }
QPushButton:hover { background:#1d4ed8; }
QListWidget#nav { background:#0b1220; border:0; font-size:14px; }
QListWidget#nav::item { padding:12px 16px; }
QListWidget#nav::item:selected { background:#1f2937; color:#fff; }
QLabel { background:transparent; }
QTabWidget::pane { border:1px solid #374151; }
QTabBar::tab { background:#1f2937; color:#cbd5e1; padding:6px 16px;
  border:1px solid #374151; border-bottom:0; }
QTabBar::tab:selected { background:#2563eb; color:#fff; }
QWidget#header { background:#0b1220; border-bottom:1px solid #1f2937; }
"""

ASSETS = Path(__file__).parent / "assets"


class MainWindow(QWidget):
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

    def _open_run(self, run: dict):
        self._viewer.load_run(run)
        self._analysis.load_run(run)
        self._nav.setCurrentRow(2)

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
