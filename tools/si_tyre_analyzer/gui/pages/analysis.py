"""Analysis page: tread profile, temps over time, and car balance."""

from __future__ import annotations

import os
from pathlib import Path

import matplotlib

matplotlib.use("QtAgg")
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from matplotlib.figure import Figure
from PySide6.QtWidgets import (
    QDoubleSpinBox,
    QFileDialog,
    QHBoxLayout,
    QLabel,
    QMessageBox,
    QPushButton,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from ...constants import WHEELS
from ...lapdata import parse_laps
from .. import meta, prefs, theme
from ..colors import tyre_cmap
from ..runs import DEFAULT_DIR, run_label
from ..widgets import RunSelector

POS = {"FL": (0, 0), "FR": (0, 1), "RL": (1, 0), "RR": (1, 1)}
LOGO = Path(__file__).resolve().parent.parent / "assets" / "si_tyre_logo.svg"
# A4 landscape in mm; matplotlib figsize is in inches, so divide by 25.4.
A4_MM = (297, 210)
A4_LANDSCAPE = (A4_MM[0] / 25.4, A4_MM[1] / 25.4)


def _svg_array(path, width=2400):
    """Render an SVG to an RGBA numpy array via Qt, or None on failure."""
    from PySide6.QtCore import QByteArray
    from PySide6.QtGui import QImage, QPainter
    from PySide6.QtSvg import QSvgRenderer

    renderer = QSvgRenderer(QByteArray(Path(path).read_bytes()))
    size = renderer.defaultSize()
    if size.width() <= 0:
        return None
    height = max(1, round(width * size.height() / size.width()))
    img = QImage(width, height, QImage.Format_RGBA8888)
    img.fill(0)
    painter = QPainter(img)
    renderer.render(painter)
    painter.end()
    return np.frombuffer(bytes(img.constBits()), dtype=np.uint8).reshape(
        height, width, 4
    )


def _bands(s):
    cols = s.grids.shape[2]
    third = max(1, cols // 3)
    by = s.grids.mean(axis=1)  # (N, cols)
    inner = by[:, :third].mean(axis=1)
    outer = by[:, cols - third :].mean(axis=1)
    midc = by[:, third : cols - third]
    middle = midc.mean(axis=1) if midc.shape[1] else by.mean(axis=1)
    return inner, middle, outer


def _crown(inner, middle, outer):
    i, m, o = inner.mean(), middle.mean(), outer.mean()
    edges = (i + o) / 2
    if m - edges > 3:
        return "centre hot"
    if edges - m > 3:
        return "shoulders hot"
    return "flat"


class AnalysisPage(QWidget):
    """Tread profile, temperature over time, time-in-window, and balance."""

    def __init__(self):
        super().__init__()
        self._run = {}
        self._laps = []

        root = QVBoxLayout(self)
        top_bar = QHBoxLayout()
        self._sel = RunSelector()
        self._sel.runChanged.connect(self._on_run)
        top_bar.addWidget(self._sel, 1)
        top_bar.addWidget(QLabel("Window °C:"))
        win_lo, win_hi = prefs.window(60, 90)
        self._lo = QDoubleSpinBox()
        self._lo.setRange(0, 200)
        self._lo.setValue(win_lo)
        self._hi = QDoubleSpinBox()
        self._hi.setRange(0, 200)
        self._hi.setValue(win_hi)
        for sb in (self._lo, self._hi):
            sb.valueChanged.connect(self._window_changed)
            top_bar.addWidget(sb)
        b_laps = QPushButton("Load lap CSV…")
        b_laps.clicked.connect(self._load_laps)
        top_bar.addWidget(b_laps)
        top_bar.addWidget(QLabel("Lap offset s:"))
        self._off = QDoubleSpinBox()
        self._off.setRange(-3600, 3600)
        self._off.setSingleStep(1)
        self._off.valueChanged.connect(self._on_offset)
        top_bar.addWidget(self._off)
        self._lapinfo = QLabel("no laps")
        self._lapinfo.setStyleSheet(f"color:{theme.MUTED};")
        top_bar.addWidget(self._lapinfo)
        b_export = QPushButton("Export report…")
        b_export.clicked.connect(self._export)
        top_bar.addWidget(b_export)
        root.addLayout(top_bar)

        self._tabs = QTabWidget()
        self._fig_over = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_prof = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_bal = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_win = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_lap = Figure(facecolor=theme.BG, layout="constrained")
        self._c_over = FigureCanvasQTAgg(self._fig_over)
        self._c_prof = FigureCanvasQTAgg(self._fig_prof)
        self._c_bal = FigureCanvasQTAgg(self._fig_bal)
        self._c_win = FigureCanvasQTAgg(self._fig_win)
        self._c_lap = FigureCanvasQTAgg(self._fig_lap)
        self._tabs.addTab(
            self._captioned(
                self._c_over,
                "Tyre temperature over the run vs the target window.",
            ),
            "Over time",
        )
        self._tabs.addTab(
            self._captioned(
                self._c_win,
                "Share of the run each tyre spent below, inside, and above the "
                "target window.",
            ),
            "In window",
        )
        self._tabs.addTab(
            self._captioned(
                self._c_prof,
                "Across-tread temperature (inner / middle / outer) per tyre.",
            ),
            "Profile",
        )
        self._tabs.addTab(
            self._captioned(
                self._c_bal,
                "Run-average temperature per wheel — front/rear & left/right balance.",
            ),
            "Balance",
        )
        self._tabs.addTab(self._lap_panel(), "Per lap")
        root.addWidget(self._tabs, 1)

    def _lap_panel(self):
        w = QWidget()
        v = QVBoxLayout(w)
        v.setContentsMargins(6, 6, 6, 6)
        cap = QLabel("Average tread temperature per lap per wheel.")
        cap.setStyleSheet(f"color:{theme.MUTED};")
        cap.setWordWrap(True)
        v.addWidget(cap)
        row = QHBoxLayout()
        row.addWidget(QLabel("Bands:"))
        self._band_btns = {}
        for name, default in (("inner", False), ("mid", True), ("outer", False)):
            b = QPushButton(name)
            b.setCheckable(True)
            b.setChecked(default)
            b.setMaximumWidth(64)
            b.toggled.connect(self._plot_laps)
            self._band_btns[name] = b
            row.addWidget(b)
        row.addStretch(1)
        v.addLayout(row)
        v.addWidget(self._c_lap, 1)
        return w

    def _captioned(self, canvas, text):
        w = QWidget()
        v = QVBoxLayout(w)
        v.setContentsMargins(6, 6, 6, 6)
        cap = QLabel(text)
        cap.setStyleSheet(f"color:{theme.MUTED};")
        cap.setWordWrap(True)
        v.addWidget(cap)
        v.addWidget(canvas, 1)
        return w

    # ---- loading ----
    def _window_changed(self):
        prefs.set_window(self._lo.value(), self._hi.value())
        self._replot()

    def load_run(self, run: dict):
        self._sel.load_run(run)

    def _on_run(self, run: dict):
        self._run = run
        self._replot()

    def _load_laps(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Lap timing CSV", prefs.last_dir(DEFAULT_DIR), "CSV (*.csv)"
        )
        if not path:
            return
        self._laps = parse_laps(path)
        self._lapinfo.setText(
            f"{len(self._laps)} laps" if self._laps else "no laps parsed"
        )
        self._replot()

    # ---- export ----
    def _tab_figs(self):
        # Same order as the tabs were added.
        return [
            self._fig_over,
            self._fig_win,
            self._fig_prof,
            self._fig_bal,
            self._fig_lap,
        ]

    def _cover_fig(self, sid):
        m = meta.load(prefs.last_dir(DEFAULT_DIR), sid) if sid else {}
        fig = Figure(figsize=A4_LANDSCAPE, facecolor=theme.BG)

        arr = _svg_array(str(LOGO))
        if arr is not None:
            ax_logo = fig.add_axes([0.0, 0.80, 1.0, 0.14])
            ax_logo.imshow(arr, aspect="equal")
            ax_logo.set_anchor("C")
            ax_logo.axis("off")

        ax = fig.add_axes([0.14, 0.10, 0.72, 0.62])
        ax.axis("off")
        ax.text(
            0.0,
            1.0,
            run_label(self._run),
            transform=ax.transAxes,
            color=theme.TEXT,
            fontsize=22,
            fontweight="bold",
            va="top",
        )
        ax.axhline(0.93, color=theme.BORDER, lw=1)

        pressures = "    ".join(
            f"{p.upper()} {m[k]}"
            for p, k in (("fl", "p_fl"), ("fr", "p_fr"), ("rl", "p_rl"), ("rr", "p_rr"))
            if m.get(k)
        )
        rows = [
            ("Date", m.get("date")),
            ("Track", m.get("track")),
            ("Driver", m.get("driver")),
            ("Compound", m.get("compound")),
            ("Ambient °C", m.get("ambient")),
            ("Pressures", pressures),
        ]
        y = 0.82
        for label, val in rows:
            if not val:
                continue
            ax.text(
                0.0, y, label, transform=ax.transAxes, color=theme.MUTED, fontsize=13
            )
            ax.text(
                0.22, y, str(val), transform=ax.transAxes, color=theme.TEXT, fontsize=13
            )
            y -= 0.08
        if m.get("notes"):
            y -= 0.02
            ax.text(
                0.0, y, "Notes", transform=ax.transAxes, color=theme.MUTED, fontsize=13
            )
            ax.text(
                0.22,
                y,
                m["notes"],
                transform=ax.transAxes,
                color=theme.TEXT,
                fontsize=13,
                va="top",
                wrap=True,
            )
        return fig

    def _export(self):
        if not self._run:
            QMessageBox.information(self, "Export", "Load a run first.")
            return
        present = [s for s in self._run.values() if s is not None]
        sid = present[0].session_id if present else None
        base = f"tyre_report_session_{sid}" if sid else "tyre_report"
        default = os.path.join(prefs.last_dir(DEFAULT_DIR), base)
        path, sel = QFileDialog.getSaveFileName(
            self,
            "Export report",
            default,
            "PDF report (*.pdf);;PNG image (*.png)",
        )
        if not path:
            return
        is_png = "png" in sel.lower() or path.lower().endswith(".png")
        if is_png and not path.lower().endswith(".png"):
            path += ".png"
        if not is_png and not path.lower().endswith(".pdf"):
            path += ".pdf"
        try:
            if is_png:
                fig = self._tab_figs()[self._tabs.currentIndex()]
                self._save_a4(fig, path=path)
            else:
                with PdfPages(path) as pdf:
                    self._save_a4(self._cover_fig(sid), pdf=pdf)
                    for fig in self._tab_figs():
                        self._save_a4(fig, pdf=pdf)
        except OSError as e:
            QMessageBox.warning(self, "Export failed", str(e))
            return
        QMessageBox.information(self, "Export", f"Saved {path}")

    @staticmethod
    def _save_a4(fig, pdf=None, path=None):
        # Force a fixed A4 size + dpi so pages render identically regardless
        # of whether the tab was ever shown (its live canvas size varies).
        old = fig.get_size_inches()
        fig.set_size_inches(A4_LANDSCAPE)
        if pdf is not None:
            pdf.savefig(fig, facecolor=fig.get_facecolor(), dpi=200)
        else:
            fig.savefig(path, facecolor=fig.get_facecolor(), dpi=200)
        fig.set_size_inches(old)

    def _on_offset(self):
        if not self._run:
            return
        self._plot_over()
        self._plot_laps()

    # ---- plotting ----
    def _replot(self):
        if not self._run:
            return
        self._plot_profile()
        self._plot_over()
        self._plot_balance()
        self._plot_window()
        self._plot_laps()

    def _axes(self, fig):
        fig.clear()
        ax = {}
        for w, (r, c) in POS.items():
            a = fig.add_subplot(2, 2, r * 2 + c + 1, facecolor=theme.SURFACE)
            a.set_title(w, color=theme.TEXT, fontsize=10)
            a.tick_params(colors=theme.MUTED, labelsize=8)
            for sp in a.spines.values():
                sp.set_color(theme.BORDER)
            ax[w] = a
        return ax

    def _plot_profile(self):
        ax = self._axes(self._fig_prof)
        for w, a in ax.items():
            s = self._run.get(w)
            if s is None:
                continue
            i, m, o = _bands(s)
            vals = [i.mean(), m.mean(), o.mean()]
            a.bar(
                ["inner", "mid", "outer"],
                vals,
                color=[theme.INNER, theme.MID_BAR, theme.OUTER],
            )
            a.set_title(
                f"{w}  inner−outer {vals[0] - vals[2]:+.1f}°  ·  {_crown(i, m, o)}",
                color=theme.TEXT,
                fontsize=9,
            )
            a.set_ylabel("°C", color=theme.MUTED, fontsize=8)
        self._c_prof.draw_idle()

    def _plot_over(self):
        lo, hi = self._lo.value(), self._hi.value()
        ax = self._axes(self._fig_over)
        for w, a in ax.items():
            s = self._run.get(w)
            if s is None:
                continue
            t = s.t_offsets_ms / 1000.0
            i, m, o = _bands(s)
            a.axhspan(lo, hi, color=theme.IN_WINDOW, alpha=0.10)
            a.plot(t, i, label="inner", lw=1.2, color=theme.INNER)
            a.plot(t, m, label="mid", lw=1.2, color=theme.MID)
            a.plot(t, o, label="outer", lw=1.2, color=theme.OUTER)
            off = self._off.value()
            ytop = a.get_ylim()[1]
            for lap in self._laps:
                x = lap.start_s + off
                if t[0] <= x <= t[-1]:
                    a.axvline(x, color=theme.MUTED_2, lw=0.6, alpha=0.6)
                    a.text(
                        x,
                        ytop,
                        str(lap.lap),
                        color=theme.MUTED,
                        fontsize=6,
                        ha="left",
                        va="top",
                    )
            inwin = np.mean((m >= lo) & (m <= hi)) * 100
            a.set_title(f"{w}  {inwin:.0f}% in window", color=theme.TEXT, fontsize=9)
            a.legend(
                fontsize=6, loc="upper right", labelcolor=theme.TEXT, facecolor=theme.BG
            )
        self._c_over.draw_idle()

    def _plot_window(self):
        lo, hi = self._lo.value(), self._hi.value()
        self._fig_win.clear()
        a = self._fig_win.add_subplot(111, facecolor=theme.SURFACE)
        present = [w for w in WHEELS if self._run.get(w) is not None]
        y = range(len(present))
        for k, w in enumerate(present):
            _, m, _ = _bands(self._run[w])
            below = float(np.mean(m < lo)) * 100
            inw = float(np.mean((m >= lo) & (m <= hi))) * 100
            above = float(np.mean(m > hi)) * 100
            a.barh(k, below, color=theme.INNER)
            a.barh(k, inw, left=below, color=theme.IN_WINDOW)
            a.barh(k, above, left=below + inw, color=theme.OUTER)
            a.text(
                inw / 2 + below,
                k,
                f"{inw:.0f}%",
                va="center",
                ha="center",
                color=theme.BG_DEEP,
                fontsize=9,
                fontweight="bold",
            )
        a.set_yticks(list(y))
        a.set_yticklabels(present, color=theme.TEXT)
        a.set_xlim(0, 100)
        a.set_xlabel("% of run", color=theme.MUTED)
        a.tick_params(colors=theme.MUTED)
        for sp in a.spines.values():
            sp.set_color(theme.BORDER)
        a.legend(
            ["below", "in window", "above"],
            fontsize=7,
            ncol=3,
            loc="upper center",
            labelcolor=theme.TEXT,
            facecolor=theme.BG,
        )
        self._c_win.draw_idle()

    def _plot_balance(self):
        cmap = tyre_cmap()
        avgs = {}
        for w in WHEELS:
            s = self._run.get(w)
            avgs[w] = float(s.grids.mean()) if s is not None else float("nan")
        allv = [v for v in avgs.values() if not np.isnan(v)]
        vmin, vmax = (min(allv), max(allv)) if allv else (0, 1)

        ax = self._axes(self._fig_bal)
        for w, a in ax.items():
            s = self._run.get(w)
            if s is None:
                a.set_axis_off()
                continue
            a.imshow(
                s.grids.mean(axis=0), cmap=cmap, vmin=vmin, vmax=vmax, aspect="auto"
            )
            a.set_xticks([])
            a.set_yticks([])
            a.set_title(f"{w}  {avgs[w]:.1f}°", color=theme.TEXT, fontsize=9)

        def avg(*ws):
            xs = [avgs[w] for w in ws if not np.isnan(avgs[w])]
            return sum(xs) / len(xs) if xs else float("nan")

        fr = avg("FL", "FR") - avg("RL", "RR")
        lr = avg("FL", "RL") - avg("FR", "RR")
        self._fig_bal.suptitle(
            f"front−rear {fr:+.1f}°    left−right {lr:+.1f}°",
            color=theme.TEXT,
            fontsize=10,
        )
        self._c_bal.draw_idle()

    def _lap_avg(self, t, arr, off):
        nums = []
        temps = []
        for k, lap in enumerate(self._laps):
            start = lap.start_s + off
            if k + 1 < len(self._laps):
                end = self._laps[k + 1].start_s + off
            else:
                end = t[-1] + 1.0
            mask = (t >= start) & (t < end)
            if mask.any():
                nums.append(lap.lap)
                temps.append(float(arr[mask].mean()))
        return nums, temps

    def _plot_laps(self):
        self._fig_lap.clear()
        a = self._fig_lap.add_subplot(111, facecolor=theme.SURFACE)
        a.tick_params(colors=theme.MUTED)
        for sp in a.spines.values():
            sp.set_color(theme.BORDER)
        if not self._laps:
            a.text(
                0.5,
                0.5,
                "Load a lap CSV",
                color=theme.MUTED,
                ha="center",
                va="center",
                transform=a.transAxes,
            )
            self._c_lap.draw_idle()
            return
        off = self._off.value()
        colors = {
            "FL": theme.INNER,
            "FR": theme.OUTER,
            "RL": theme.IN_WINDOW,
            "RR": theme.MID,
        }
        # (band name, index into _bands, line style)
        styles = (("inner", 0, "--"), ("mid", 1, "-"), ("outer", 2, ":"))
        bands = [b for b in styles if self._band_btns[b[0]].isChecked()]
        multi = len(bands) > 1
        for w in WHEELS:
            s = self._run.get(w)
            if s is None:
                continue
            t = s.t_offsets_ms / 1000.0
            tri = _bands(s)
            for name, idx, ls in bands:
                nums, temps = self._lap_avg(t, tri[idx], off)
                if temps:
                    a.plot(
                        nums,
                        temps,
                        marker="o",
                        lw=1.3,
                        ls=ls,
                        color=colors.get(w),
                        label=f"{w} {name}" if multi else w,
                    )
        a.set_xlabel("lap", color=theme.MUTED)
        a.set_ylabel("°C", color=theme.MUTED)
        a.legend(fontsize=7, ncol=4, labelcolor=theme.TEXT, facecolor=theme.BG)
        self._c_lap.draw_idle()
