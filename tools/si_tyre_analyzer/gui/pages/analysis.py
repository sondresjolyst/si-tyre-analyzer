"""Analysis page: tread profile, temps over time, and car balance."""

from __future__ import annotations

import os
from datetime import date as _date
from pathlib import Path

import matplotlib

matplotlib.use("QtAgg")
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from matplotlib.figure import Figure
from matplotlib.patches import FancyBboxPatch
from PySide6.QtCore import QSize
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

from ...constants import OPT_HI_DEFAULT, OPT_LO_DEFAULT, WHEELS
from ...lapdata import parse_laps
from .. import meta, prefs, theme
from ..colors import heat_rgb, scale_hi, scale_lo, tyre_cmap
from ..icons import icon, tool
from ..runs import DEFAULT_DIR, run_label
from ..widgets import RunSelector

POS = {"FL": (0, 0), "FR": (0, 1), "RL": (1, 0), "RR": (1, 1)}
LOGO = Path(__file__).resolve().parent.parent / "assets" / "si_tyre_logo.svg"
# A4 landscape in mm; matplotlib figsize is in inches, so divide by 25.4.
A4_MM = (297, 210)
A4_LANDSCAPE = (A4_MM[0] / 25.4, A4_MM[1] / 25.4)
EXPORT_DPI = 200
# (left, bottom, width, height) constrained-layout rects. On screen only a thin
# top band is reserved for the legend so graphs stay large; export adds side and
# bottom margins plus room for the page title + footer.
SCREEN_RECT = (0, 0, 1, 0.90)
REPORT_RECT = (0.03, 0.07, 0.94, 0.82)


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


class AnalysisPage(QWidget):
    """Tread profile, temperature over time, time-in-window, and balance."""

    # (icon, label) per tab, in tab order — also used to build the sidebar.
    SECTIONS = [
        ("trend", "Over time"),
        ("profile", "Profile"),
        ("balance", "Balance"),
        ("flag", "Per lap"),
    ]

    def set_section(self, index: int) -> None:
        self._tabs.setCurrentIndex(index)

    def __init__(self):
        super().__init__()
        theme.apply_matplotlib()
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
            sb.setKeyboardTracking(False)
            sb.valueChanged.connect(self._window_changed)
            top_bar.addWidget(sb)
        top_bar.addWidget(tool("flag", "Load lap CSV…", self._load_laps))
        top_bar.addWidget(QLabel("Lap offset s:"))
        self._off = QDoubleSpinBox()
        self._off.setRange(-3600, 3600)
        self._off.setSingleStep(1)
        self._off.setKeyboardTracking(False)
        self._off.valueChanged.connect(self._on_offset)
        top_bar.addWidget(self._off)
        self._lapinfo = QLabel("no laps")
        self._lapinfo.setStyleSheet(f"color:{theme.MUTED};")
        top_bar.addWidget(self._lapinfo)
        top_bar.addWidget(tool("export", "Export report…", self._export))
        root.addLayout(top_bar)

        self._tabs = QTabWidget()
        self._tabs.setIconSize(QSize(16, 16))
        self._tabs.tabBar().hide()  # sections live in the sidebar now
        self._fig_over = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_prof = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_bal = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_lap = Figure(facecolor=theme.BG, layout="constrained")
        self._c_over = FigureCanvasQTAgg(self._fig_over)
        self._c_prof = FigureCanvasQTAgg(self._fig_prof)
        self._c_bal = FigureCanvasQTAgg(self._fig_bal)
        self._c_lap = FigureCanvasQTAgg(self._fig_lap)
        self._tabs.addTab(
            self._captioned(self._c_over, "Tyre temperature over the run."),
            icon("trend"),
            "Over time",
        )
        self._tabs.addTab(
            self._captioned(
                self._c_prof,
                "Average temperature across the tyre width — inner edge, middle, "
                "outer edge — per wheel.",
            ),
            icon("profile"),
            "Profile",
        )
        self._tabs.addTab(
            self._captioned(
                self._c_bal,
                "Run-average temperature per wheel — front/rear & left/right balance.",
            ),
            icon("balance"),
            "Balance",
        )
        self._tabs.addTab(self._lap_panel(), icon("flag"), "Per lap")
        root.addWidget(self._tabs, 1)

    def _lap_panel(self):
        w = QWidget()
        v = QVBoxLayout(w)
        v.setContentsMargins(14, 12, 14, 12)
        v.setSpacing(8)
        cap = QLabel("Average tread temperature per lap per wheel.")
        cap.setStyleSheet(f"color:{theme.MUTED};")
        cap.setWordWrap(True)
        v.addWidget(cap)
        row = QHBoxLayout()
        row.addWidget(QLabel("Bands:"))
        self._band_btns = {}
        for name, default in (("inner", False), ("mid", True), ("outer", False)):
            b = QPushButton(name)
            b.setObjectName("toggle")
            b.setCheckable(True)
            b.setChecked(default)
            b.setMaximumWidth(72)
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
        v.setContentsMargins(14, 12, 14, 12)
        v.setSpacing(8)
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
            self._fig_prof,
            self._balance_fig(),
            self._fig_lap,
        ]

    def _cover_fig(self, sid):
        m = meta.load(prefs.last_dir(DEFAULT_DIR), sid) if sid else {}
        fig = Figure(figsize=A4_LANDSCAPE, facecolor=theme.BG)

        arr = _svg_array(str(LOGO))
        if arr is not None:
            fig_w, fig_h = A4_LANDSCAPE
            band_h = 0.12
            aspect = arr.shape[1] / arr.shape[0]
            wf = min((band_h * fig_h * aspect) / fig_w, 0.6)
            ax_logo = fig.add_axes([(1.0 - wf) / 2.0, 0.82, wf, band_h])
            ax_logo.imshow(arr, aspect="auto")
            ax_logo.axis("off")

        ax = fig.add_axes([0.14, 0.10, 0.72, 0.62])
        ax.axis("off")
        ax.text(
            0.0,
            1.06,
            "TYRE TEMPERATURE REPORT",
            transform=ax.transAxes,
            color=theme.ACCENT,
            fontsize=11,
            fontweight="bold",
            va="top",
        )
        ax.text(
            0.0,
            1.0,
            run_label(self._run) or "Tyre session",
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
                self._save_pdf(path, sid)
        except OSError as e:
            QMessageBox.warning(self, "Export failed", str(e))
            return
        QMessageBox.information(self, "Export", f"Saved {path}")

    _PAGE_TITLES = [
        "Tyre temperature over the run",
        "Tread profile across the tyre width",
        "Run balance",
        "Temperature per lap",
    ]

    def _save_pdf(self, path, sid):
        run = run_label(self._run) or "Tyre session"
        pages = list(zip(self._tab_figs(), self._PAGE_TITLES))
        if not self._laps:  # skip the empty Per-lap page
            pages = [p for p in pages if p[1] != "Temperature per lap"]
        total = len(pages) + 1
        with PdfPages(path) as pdf:
            cover = self._cover_fig(sid)
            arts = self._foot(cover, f"{run}  ·  1 / {total}")
            self._save_a4(cover, pdf=pdf)
            for a in arts:
                a.remove()
            for i, (fig, title) in enumerate(pages, start=2):
                cleanup = self._decorate(fig, title, f"{run}  ·  {i} / {total}")
                self._save_a4(fig, pdf=pdf)
                cleanup()

    @staticmethod
    def _foot(fig, right):
        return [
            fig.text(
                0.035,
                0.02,
                f"Generated {_date.today().isoformat()}",
                color=theme.MUTED_2,
                fontsize=8,
                ha="left",
            ),
            fig.text(0.965, 0.02, right, color=theme.MUTED_2, fontsize=8, ha="right"),
        ]

    def _decorate(self, fig, title, footer):
        arts = [
            fig.suptitle(
                title,
                x=0.045,
                y=0.975,
                ha="left",
                color=theme.TEXT,
                fontsize=14,
                fontweight="bold",
            ),
            *self._foot(fig, footer),
        ]
        constrained = (
            type(fig.get_layout_engine()).__name__ == "ConstrainedLayoutEngine"
        )
        if constrained:
            fig.set_layout_engine(
                "constrained", rect=REPORT_RECT, w_pad=0.08, h_pad=0.08
            )

        def cleanup():
            for a in arts:
                a.remove()
            if constrained:
                fig.set_layout_engine(
                    "constrained", rect=SCREEN_RECT, w_pad=0.04, h_pad=0.04
                )

        return cleanup

    @staticmethod
    def _save_a4(fig, pdf=None, path=None):
        # Force a fixed A4 size + dpi so pages render identically regardless
        # of whether the tab was ever shown (its live canvas size varies).
        old = fig.get_size_inches()
        fig.set_size_inches(A4_LANDSCAPE)
        if pdf is not None:
            pdf.savefig(fig, facecolor=fig.get_facecolor(), dpi=EXPORT_DPI)
        else:
            fig.savefig(path, facecolor=fig.get_facecolor(), dpi=EXPORT_DPI)
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
        self._plot_laps()

    @staticmethod
    def _style(a, grid: bool = True):
        a.set_facecolor(theme.SURFACE)
        a.tick_params(colors=theme.MUTED, labelsize=8)
        a.spines["top"].set_visible(False)
        a.spines["right"].set_visible(False)
        a.spines["left"].set_color(theme.BORDER)
        a.spines["bottom"].set_color(theme.BORDER)
        if grid:
            a.grid(True, axis="y", color=theme.BORDER, alpha=0.35, linewidth=0.6)
            a.set_axisbelow(True)

    def _axes(self, fig):
        fig.clear()
        ax = {}
        for w, (r, c) in POS.items():
            a = fig.add_subplot(2, 2, r * 2 + c + 1)
            self._style(a)
            # Wheel always top-left; each plot fills the metric top-right.
            a.set_title(w, loc="left", color=theme.TEXT, fontweight="bold")
            ax[w] = a
        return ax

    @staticmethod
    def _metric(a, text):
        a.set_title(text, loc="right", color=theme.MUTED, fontsize=8)

    def _plot_profile(self):
        self._fig_prof.set_layout_engine(
            "constrained", rect=SCREEN_RECT, w_pad=0.04, h_pad=0.04
        )
        ax = self._axes(self._fig_prof)
        for w, a in ax.items():
            s = self._run.get(w)
            if s is None:
                continue
            i, m, o = _bands(s)
            vals = [i.mean(), m.mean(), o.mean()]
            bars = a.bar(
                ["inner", "mid", "outer"],
                vals,
                color=[theme.INNER, theme.MID_BAR, theme.OUTER],
            )
            a.bar_label(
                bars,
                fmt="%.0f°",
                color=theme.TEXT,
                fontsize=9,
                padding=2,
                fontfamily="monospace",
            )
            edges = (vals[0] + vals[2]) / 2
            self._metric(
                a,
                f"in−out {vals[0] - vals[2]:+.1f}°   mid−edge {vals[1] - edges:+.1f}°",
            )
            a.set_ylabel("°C", color=theme.MUTED, fontsize=8)
            a.set_ylim(0, scale_hi(*self._window()))
        self._c_prof.draw_idle()

    def _plot_over(self):
        lo, hi = self._lo.value(), self._hi.value()
        off = self._off.value()
        self._fig_over.set_layout_engine(
            "constrained", rect=SCREEN_RECT, w_pad=0.04, h_pad=0.04
        )
        ax = self._axes(self._fig_over)
        handles = None
        data = {}
        gmin, gmax = lo, hi
        for w, a in ax.items():
            s = self._run.get(w)
            if s is None:
                continue
            i, m, o = _bands(s)
            data[w] = (s.t_offsets_ms / 1000.0, i, m, o)
            gmin = min(gmin, i.min(), m.min(), o.min())
            gmax = max(gmax, i.max(), m.max(), o.max())
        pad = max(2.0, (gmax - gmin) * 0.08)
        ylo, yhi = gmin - pad, gmax + pad
        for w, a in ax.items():
            if w not in data:
                continue
            t, i, m, o = data[w]
            a.axhspan(lo, hi, color=theme.IN_WINDOW, alpha=0.10)
            a.plot(t, i, label="inner", lw=1.4, color=theme.INNER)
            a.plot(t, m, label="mid", lw=1.4, color=theme.MID)
            a.plot(t, o, label="outer", lw=1.4, color=theme.OUTER)
            handles = a.get_legend_handles_labels()
            a.set_ylim(ylo, yhi)
            for lap in self._laps:
                x = lap.start_s + off
                if t[0] <= x <= t[-1]:
                    a.axvline(x, color=theme.MUTED_2, lw=0.6, alpha=0.6)
                    a.text(
                        x,
                        yhi,
                        str(lap.lap),
                        color=theme.MUTED,
                        fontsize=6,
                        ha="left",
                        va="top",
                    )
            temp = self._run[w].grids.mean(axis=(1, 2))
            below = float(np.mean(temp < lo)) * 100
            inw = float(np.mean((temp >= lo) & (temp <= hi))) * 100
            above = float(np.mean(temp > hi)) * 100
            self._metric(a, f"below {below:.0f}% · in {inw:.0f}% · above {above:.0f}%")
        if handles:
            self._fig_over.legend(
                *handles,
                loc="upper center",
                bbox_to_anchor=(0.5, 0.95),
                ncol=3,
                fontsize=9,
                labelcolor=theme.TEXT,
                frameon=False,
            )
        self._c_over.draw_idle()

    # Tile corner (x, y) per wheel — laid out like the car seen from above.
    # Portrait tile (0.78 : 1) to match the Live/Viewer patch shape; the wide
    # gap between columns matches the figure ratio so the card fills the width.
    _CAR = {
        "FL": (0.55, 1.15),
        "FR": (2.23, 1.15),
        "RL": (0.55, 0.05),
        "RR": (2.23, 0.05),
    }
    _TILE = (0.78, 1.0)

    def _plot_balance(self):
        self._fig_bal.clear()
        self._fig_bal.set_layout_engine("none")
        ax = self._fig_bal.add_axes([0.03, 0.12, 0.94, 0.80])
        ax.set_xlim(0, 3.56)
        ax.set_ylim(0, 2.3)
        ax.set_aspect("equal")
        ax.axis("off")

        avgs = {}
        tw, th = self._TILE
        for w, (x, y) in self._CAR.items():
            s = self._run.get(w)
            ax.text(
                x + 0.06,
                y + th - 0.07,
                w,
                color=theme.TEXT,
                fontsize=11,
                fontweight="bold",
                va="top",
                ha="left",
                zorder=3,
            )
            if s is None:
                ax.add_patch(
                    FancyBboxPatch(
                        (x, y),
                        tw,
                        th,
                        boxstyle="round,pad=0,rounding_size=0.12",
                        facecolor=theme.SURFACE,
                        edgecolor=theme.BORDER,
                        lw=1,
                    )
                )
                continue
            av = float(s.grids.mean())
            avgs[w] = av
            rr, gg, bb = heat_rgb(av, *self._window())
            ax.add_patch(
                FancyBboxPatch(
                    (x, y),
                    tw,
                    th,
                    boxstyle="round,pad=0,rounding_size=0.12",
                    facecolor=(rr / 255, gg / 255, bb / 255),
                    edgecolor=theme.BORDER,
                    lw=1,
                )
            )
            ax.text(
                x + tw / 2,
                y + th / 2 - 0.04,
                f"{av:.0f}°",
                color=theme.WHITE,
                fontsize=26,
                fontweight="bold",
                fontfamily="monospace",
                ha="center",
                va="center",
                zorder=3,
            )

        def avg(*ws):
            xs = [avgs[w] for w in ws if w in avgs]
            return sum(xs) / len(xs) if xs else float("nan")

        fr = avg("FL", "FR") - avg("RL", "RR")
        lr = avg("FL", "RL") - avg("FR", "RR")
        ax.text(
            1.78,
            1.05,
            f"front−rear\n{fr:+.1f}°\n\nleft−right\n{lr:+.1f}°",
            color=theme.TEXT_DIM,
            fontsize=9,
            ha="center",
            va="center",
            linespacing=1.4,
        )

        self._balance_legend()
        self._c_bal.draw_idle()

    def _window(self):
        for s in (self._run or {}).values():
            if s is not None:
                return s.opt_lo, s.opt_hi
        return OPT_LO_DEFAULT, OPT_HI_DEFAULT

    def _balance_legend(self):
        opt_lo, opt_hi = self._window()
        lo, hi = scale_lo(opt_lo, opt_hi), scale_hi(opt_lo, opt_hi)
        cax = self._fig_bal.add_axes([0.34, 0.055, 0.32, 0.012])
        grad = np.linspace(0, 1, 256)[None, :]
        cax.imshow(
            grad,
            aspect="auto",
            cmap=tyre_cmap(opt_lo, opt_hi),
            extent=[lo, hi, 0, 1],
        )
        cax.set_yticks([])
        cax.set_xticks([lo, (lo + hi) / 2, hi])
        cax.tick_params(colors=theme.MUTED, labelsize=8, length=0)
        for sp in cax.spines.values():
            sp.set_color(theme.BORDER)

    def _balance_fig(self):
        return self._fig_bal

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
        self._fig_lap.set_layout_engine(
            "constrained", rect=SCREEN_RECT, w_pad=0.04, h_pad=0.04
        )
        a = self._fig_lap.add_subplot(111)
        self._style(a)
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
        colors = theme.WHEEL_COLORS
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
        self._fig_lap.legend(
            ncol=4,
            fontsize=9,
            loc="upper center",
            bbox_to_anchor=(0.5, 0.95),
            labelcolor=theme.TEXT,
            frameon=False,
        )
        self._c_lap.draw_idle()
