"""Dark colour palette and the application stylesheet."""

from __future__ import annotations

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
QLineEdit, QComboBox, QListWidget, QTableWidget {{
  background:{SURFACE}; border:1px solid {BORDER}; border-radius:6px; padding:4px; }}
QPushButton {{ background:{ACCENT}; color:{WHITE}; border:0; border-radius:6px;
  padding:6px 12px; font-weight:600; }}
QPushButton:hover {{ background:{ACCENT_HOVER}; }}
QListWidget#nav {{ background:{BG_DEEP}; border:0; font-size:14px; }}
QListWidget#nav::item {{ padding:12px 16px; }}
QListWidget#nav::item:selected {{ background:{SURFACE}; color:{WHITE}; }}
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
