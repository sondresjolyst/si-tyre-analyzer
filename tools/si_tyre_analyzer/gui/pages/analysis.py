"""Analysis page: tread profile, temps over time, and car balance."""

from __future__ import annotations

import matplotlib

matplotlib.use("QtAgg")
import numpy as np
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
from matplotlib.figure import Figure
from PySide6.QtWidgets import (
    QDoubleSpinBox,
    QFileDialog,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from ...constants import WHEELS
from ...lapdata import parse_laps
from .. import prefs, theme
from ..colors import tyre_cmap
from ..widgets import RunSelector

POS = {"FL": (0, 0), "FR": (0, 1), "RL": (1, 0), "RR": (1, 1)}


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
        self._off.valueChanged.connect(self._plot_over)
        top_bar.addWidget(self._off)
        self._lapinfo = QLabel("no laps")
        self._lapinfo.setStyleSheet(f"color:{theme.MUTED};")
        top_bar.addWidget(self._lapinfo)
        root.addLayout(top_bar)

        self._tabs = QTabWidget()
        self._fig_over = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_prof = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_bal = Figure(facecolor=theme.BG, layout="constrained")
        self._fig_win = Figure(facecolor=theme.BG, layout="constrained")
        self._c_over = FigureCanvasQTAgg(self._fig_over)
        self._c_prof = FigureCanvasQTAgg(self._fig_prof)
        self._c_bal = FigureCanvasQTAgg(self._fig_bal)
        self._c_win = FigureCanvasQTAgg(self._fig_win)
        self._tabs.addTab(
            self._captioned(
                self._c_over,
                "Tyre temperature over the run vs the target window; lap markers "
                "from the CSV.",
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
        root.addWidget(self._tabs, 1)

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
        path, _ = QFileDialog.getOpenFileName(self, "Lap timing CSV", "", "CSV (*.csv)")
        if not path:
            return
        self._laps = parse_laps(path)
        self._lapinfo.setText(
            f"{len(self._laps)} laps" if self._laps else "no laps parsed"
        )
        self._replot()

    # ---- plotting ----
    def _replot(self):
        if not self._run:
            return
        self._plot_profile()
        self._plot_over()
        self._plot_balance()
        self._plot_window()

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
