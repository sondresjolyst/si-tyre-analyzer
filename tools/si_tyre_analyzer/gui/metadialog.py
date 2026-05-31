"""Dialog to edit a run's metadata (track, driver, compound, pressures, notes)."""

from __future__ import annotations

from PySide6.QtCore import QDate
from PySide6.QtGui import QColor, QTextCharFormat
from PySide6.QtWidgets import (
    QDateEdit,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QLineEdit,
    QPlainTextEdit,
)

from . import meta, theme

_DATE_FMT = "dd-MM-yyyy"

_CALENDAR_QSS = (
    f"QCalendarWidget QWidget {{ background-color:{theme.BG};"
    f" color:{theme.TEXT}; }}"
    f"QCalendarWidget #qt_calendar_navigationbar {{"
    f" background-color:{theme.SURFACE}; }}"
    f"QCalendarWidget QToolButton {{ color:{theme.TEXT};"
    f" background-color:{theme.SURFACE}; }}"
    f"QCalendarWidget QToolButton:hover {{ background-color:{theme.BORDER}; }}"
    f"QCalendarWidget QMenu {{ color:{theme.TEXT};"
    f" background-color:{theme.SURFACE}; }}"
    f"QCalendarWidget QSpinBox {{ color:{theme.TEXT};"
    f" background-color:{theme.SURFACE}; }}"
    f"QCalendarWidget QAbstractItemView:enabled {{ color:{theme.TEXT};"
    f" background-color:{theme.BG};"
    f" selection-background-color:{theme.ACCENT};"
    f" selection-color:{theme.WHITE}; outline:0; }}"
    f"QCalendarWidget QAbstractItemView:disabled {{ color:{theme.MUTED_2}; }}"
)


def _style_calendar(cal):
    cal.setStyleSheet(_CALENDAR_QSS)
    hdr = QTextCharFormat()
    hdr.setForeground(QColor(theme.TEXT))
    hdr.setBackground(QColor(theme.SURFACE))
    cal.setHeaderTextFormat(hdr)


class MetaDialog(QDialog):
    """Form over meta.FIELDS; returns the entered values via data()."""

    def __init__(self, data: dict, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Run info")
        self.setMinimumWidth(360)
        form = QFormLayout(self)
        self._fields = {}
        for key, label in meta.FIELDS.items():
            if key == "date":
                w = QDateEdit()
                w.setCalendarPopup(True)
                w.setDisplayFormat(_DATE_FMT)
                qd = QDate.fromString(str(data.get(key, "")), _DATE_FMT)
                w.setDate(qd if qd.isValid() else QDate.currentDate())
                _style_calendar(w.calendarWidget())
            elif key == "notes":
                w = QPlainTextEdit(str(data.get(key, "")))
                w.setFixedHeight(90)
            else:
                w = QLineEdit(str(data.get(key, "")))
            self._fields[key] = w
            form.addRow(label, w)
        buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        form.addRow(buttons)

    def data(self) -> dict:
        out = {}
        for key, w in self._fields.items():
            if isinstance(w, QDateEdit):
                out[key] = w.date().toString(_DATE_FMT)
            elif isinstance(w, QPlainTextEdit):
                out[key] = w.toPlainText().strip()
            else:
                out[key] = w.text().strip()
        return out
