"""SI Tyre Analyzer — desktop app entry point (PySide6)."""

from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtCore import QSize, Qt, QTimer
from PySide6.QtGui import QIcon, QKeySequence, QShortcut
from PySide6.QtSvgWidgets import QSvgWidget
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QLabel,
    QMessageBox,
    QPushButton,
    QStackedWidget,
    QTreeWidget,
    QTreeWidgetItem,
    QVBoxLayout,
    QWidget,
)

from . import prefs, theme, update
from .icons import icon, tool
from .net import Worker
from .pages.analysis import AnalysisPage
from .pages.library import LibraryPage
from .pages.live import LivePage
from .pages.viewer import ViewerPage
from .runs import run_label
from .theme import DARK_QSS

ASSETS = Path(__file__).parent / "assets"


class MainWindow(QWidget):
    """Top-level window: header, nav, and the stacked pages."""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("SI Tyre Analyzer")
        self.setWindowIcon(QIcon(str(ASSETS / "si_tyre_mark.svg")))
        self.resize(960, 680)
        geo = prefs.geometry()
        if geo:
            self.restoreGeometry(geo)
        self.setAcceptDrops(True)

        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        header = QWidget()
        header.setObjectName("header")
        header.setFixedHeight(66)
        hb = QHBoxLayout(header)
        hb.setContentsMargins(16, 10, 16, 10)
        logo = QSvgWidget(str(ASSETS / "si_tyre_logo.svg"))
        logo.setStyleSheet("background: transparent;")
        logo_h = 46
        native = logo.renderer().defaultSize()
        aspect = native.width() / native.height() if native.height() else 1.0
        logo.setFixedSize(round(logo_h * aspect), logo_h)
        hb.addWidget(logo)
        self._run_label = QLabel("")
        self._run_label.setStyleSheet(f"color:{theme.MUTED}; font-size:14px;")
        hb.addWidget(self._run_label)
        hb.addStretch(1)
        ver = QLabel(f"v{update.current_version()}")
        ver.setStyleSheet(f"color:{theme.MUTED_2};")
        hb.addWidget(ver)
        about = tool("info", "About SI Tyre Analyzer", self._about)
        hb.addWidget(about)
        self._update_btn = QPushButton("Check for updates")
        self._update_btn.setObjectName("ghost")
        self._update_btn.clicked.connect(lambda: self._check_updates(manual=True))
        hb.addWidget(self._update_btn)
        root.addWidget(header)

        body = QHBoxLayout()
        body.setContentsMargins(0, 0, 0, 0)
        body.setSpacing(0)
        root.addLayout(body, 1)

        self._stack = QStackedWidget()
        self._live = LivePage()
        self._library = LibraryPage()
        self._viewer = ViewerPage()
        self._analysis = AnalysisPage()
        for pg in (self._library, self._live, self._viewer, self._analysis):
            self._stack.addWidget(pg)

        self._nav = QTreeWidget()
        self._nav.setObjectName("nav")
        self._nav.setFixedWidth(180)
        self._nav.setHeaderHidden(True)
        self._nav.setIndentation(16)
        self._nav.setIconSize(QSize(20, 20))
        for label, ic, page in [
            ("Sessions", "sessions", 0),
            ("Live", "live", 1),
            ("Viewer", "viewer", 2),
        ]:
            self._nav.addTopLevelItem(self._nav_item(label, ic, page))
        self._viewer_item = self._nav.topLevelItem(2)
        # Analysis is an expander only — its sections are the children.
        analysis_item = self._nav_item("Analysis", "analysis", None)
        analysis_item.setFlags(Qt.ItemIsEnabled)
        self._nav.addTopLevelItem(analysis_item)
        for i, (ic, label) in enumerate(AnalysisPage.SECTIONS):
            analysis_item.addChild(self._nav_item(label, ic, 3, i))
        analysis_item.setExpanded(False)
        body.addWidget(self._nav)
        body.addWidget(self._stack, 1)

        self._nav.currentItemChanged.connect(self._nav_changed)
        self._nav.itemClicked.connect(self._nav_clicked)
        self._nav.setCurrentItem(self._nav.topLevelItem(0))
        self._library.openRun.connect(self._open_run)

        self._add_shortcuts()
        QTimer.singleShot(1500, lambda: self._check_updates(manual=False))

    def _add_shortcuts(self):
        for n in (1, 2, 3):
            QShortcut(
                QKeySequence(f"Ctrl+{n}"),
                self,
                activated=lambda i=n - 1: self._nav.setCurrentItem(
                    self._nav.topLevelItem(i)
                ),
            )
        QShortcut(QKeySequence("Ctrl+4"), self, activated=self._open_analysis)
        QShortcut(QKeySequence("Ctrl+F"), self, activated=self._focus_search)
        QShortcut(QKeySequence("F5"), self, activated=self._library._refresh)
        QShortcut(QKeySequence("Ctrl+O"), self, activated=self._library._browse)

    def _open_analysis(self):
        analysis_item = self._nav.topLevelItem(3)
        analysis_item.setExpanded(True)
        self._nav.setCurrentItem(analysis_item.child(0))

    def _focus_search(self):
        self._nav.setCurrentItem(self._nav.topLevelItem(0))
        self._library.focus_search()

    def _about(self):
        QMessageBox.about(
            self,
            "About SI Tyre Analyzer",
            f"<b>SI Tyre Analyzer</b><br>Version {update.current_version()}<br><br>"
            f'<a href="https://github.com/{update.REPO}">'
            f"github.com/{update.REPO}</a>",
        )

    @staticmethod
    def _nav_item(label, ic, page, section=None):
        item = QTreeWidgetItem([label])
        item.setIcon(0, icon(ic))
        item.setData(0, Qt.UserRole, (page, section))
        return item

    def _nav_clicked(self, item, _col):
        page, _section = item.data(0, Qt.UserRole)
        if page is None:  # Analysis parent — toggle the section list
            item.setExpanded(not item.isExpanded())

    def _nav_changed(self, current, _previous):
        if current is None:
            return
        page, section = current.data(0, Qt.UserRole)
        if page is None:
            return
        self._stack.setCurrentIndex(page)
        if section is not None:
            self._analysis.set_section(section)

    def _open_run(self, run: dict):
        self._viewer.load_run(run)
        self._analysis.load_run(run)
        self._nav.setCurrentItem(self._viewer_item)
        label = run_label(run)
        self._run_label.setText(label)
        self.setWindowTitle(
            f"SI Tyre Analyzer — {label}" if label else "SI Tyre Analyzer"
        )

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
            "Updating",
            "The app will now close to finish updating, then reopen.",
        )
        QApplication.quit()

    def _on_update_failed(self, err: str):
        self._reset_update_btn()
        QMessageBox.warning(self, "Update failed", f"The update failed.\n\n{err}")

    @staticmethod
    def _dropped_bins(mime) -> list[str]:
        if not mime.hasUrls():
            return []
        return [
            u.toLocalFile()
            for u in mime.urls()
            if u.toLocalFile().lower().endswith(".bin")
        ]

    def dragEnterEvent(self, ev):
        if self._dropped_bins(ev.mimeData()):
            ev.acceptProposedAction()

    def dropEvent(self, ev):
        paths = self._dropped_bins(ev.mimeData())
        if paths:
            self._nav.setCurrentRow(0)
            self._library.import_files(paths)
            ev.acceptProposedAction()

    def closeEvent(self, ev):
        prefs.set_geometry(self.saveGeometry())
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
