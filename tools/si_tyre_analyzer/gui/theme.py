"""Dark colour palette and the application stylesheet."""

from __future__ import annotations

from pathlib import Path

_ICONS = Path(__file__).parent / "assets" / "icons"
_CHEVRON = (_ICONS / "chevron.svg").as_posix()
_CHEVRON_UP = (_ICONS / "chevron-up.svg").as_posix()
_CHEVRON_RIGHT = (_ICONS / "chevron-right.svg").as_posix()

# Surfaces / chrome
BG_DEEP = "#0b1220"
BG = "#111827"
SURFACE = "#1f2937"
BORDER = "#374151"

# Text
TEXT = "#f3f4f6"
TEXT_DIM = "#cbd5e1"
MUTED = "#9ca3af"
MUTED_2 = "#6b7280"
WHITE = "#ffffff"
GRID_TEXT = "#e5e7eb"

# Accent
ACCENT = "#2563eb"
ACCENT_HOVER = "#1d4ed8"

# Semantic plot colours
INNER = "#60a5fa"
MID = "#d4d4d4"
MID_BAR = "#a3a3a3"
OUTER = "#f87171"
IN_WINDOW = "#22c55e"

DARK_QSS = f"""
QWidget {{ background:{BG}; color:{TEXT}; font-size:13px; }}
QLineEdit, QComboBox, QListWidget, QTableWidget, QDateEdit, QPlainTextEdit,
QAbstractSpinBox {{
  background:{SURFACE}; border:1px solid {BORDER}; border-radius:6px;
  padding:4px 6px; }}
QPushButton {{ background:{ACCENT}; color:{WHITE}; border:0; border-radius:6px;
  padding:6px 12px; font-weight:600; }}
QPushButton:hover {{ background:{ACCENT_HOVER}; }}
QToolButton {{ background:transparent; border:1px solid {BORDER};
  border-radius:6px; padding:5px; }}
QToolButton:hover {{ background:{SURFACE}; }}
QToolButton:disabled {{ border-color:{SURFACE}; }}
QToolButton::menu-indicator {{ width:0; height:0; }}
QComboBox {{ padding-right:24px; }}
QComboBox::drop-down {{ subcontrol-origin:padding; subcontrol-position:top right;
  width:22px; border-left:1px solid {BORDER};
  border-top-right-radius:6px; border-bottom-right-radius:6px; }}
QComboBox::down-arrow {{ image:url("{_CHEVRON}"); width:12px; height:12px; }}
QComboBox QAbstractItemView {{ background:{SURFACE}; border:1px solid {BORDER};
  selection-background-color:{ACCENT}; selection-color:{WHITE}; outline:0; }}
QAbstractSpinBox {{ padding-right:22px; }}
QAbstractSpinBox::up-button {{ subcontrol-origin:padding;
  subcontrol-position:top right; width:20px; border-left:1px solid {BORDER};
  border-top-right-radius:6px; }}
QAbstractSpinBox::down-button {{ subcontrol-origin:padding;
  subcontrol-position:bottom right; width:20px; border-left:1px solid {BORDER};
  border-bottom-right-radius:6px; }}
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover {{
  background:{SURFACE}; }}
QAbstractSpinBox::up-arrow {{ image:url("{_CHEVRON_UP}"); width:11px; height:11px; }}
QAbstractSpinBox::down-arrow {{ image:url("{_CHEVRON}"); width:11px; height:11px; }}
QListWidget#nav {{ background:{BG_DEEP}; border:0; font-size:14px; }}
QListWidget#nav::item {{ padding:12px 16px; }}
QListWidget#nav::item:selected {{ background:{SURFACE}; color:{WHITE}; }}
QTreeView#nav {{ background:{BG_DEEP}; border:0; font-size:14px; outline:0; }}
QTreeView#nav::item {{ padding:10px 12px; border:0; color:{TEXT_DIM}; }}
QTreeView#nav::item:hover {{ background:{SURFACE}; }}
QTreeView#nav::item:selected {{ background:{SURFACE}; color:{WHITE}; }}
QTreeView#nav::branch {{ background:{BG_DEEP}; }}
QTreeView#nav::branch:has-children:closed {{ image:url("{_CHEVRON_RIGHT}"); }}
QTreeView#nav::branch:has-children:open {{ image:url("{_CHEVRON}"); }}
QLabel {{ background:transparent; }}
QTabWidget::pane {{ border:1px solid {BORDER}; }}
QTabBar::tab {{ background:{SURFACE}; color:{TEXT_DIM}; padding:6px 16px;
  border:1px solid {BORDER}; border-bottom:0; }}
QTabBar::tab:selected {{ background:{ACCENT}; color:{WHITE}; }}
QWidget#header {{ background:{BG_DEEP}; border-bottom:1px solid {SURFACE}; }}
QPushButton#ghost {{ background:transparent; color:{MUTED};
  border:1px solid {BORDER}; font-weight:500; }}
QPushButton#ghost:hover {{ background:{SURFACE}; color:{TEXT}; }}
"""
