"""Grid heatmap of a tyre session.

Interactive view: Play/Pause + speed (1x/2x/4x) + a time slider to scrub.
Per-column average temperatures (tread profile, inner -> outer) are shown under
each tyre. Colors match the on-device /live dashboard. --save exports an
animation.
"""

from __future__ import annotations

import numpy as np

from .gui.colors import scale_hi, scale_lo, tyre_cmap
from .logreader import SessionData


def _col_profile(grid: np.ndarray) -> str:
    """Per-column average (mean over rows), as a spaced row of integers."""
    return "  ".join(f"{v:.0f}" for v in grid.mean(axis=0))


def _add_player(fig, slider, n, base_ms):
    """Play/Pause + speed buttons that advance the slider via a timer."""
    from matplotlib.widgets import Button

    state = {"playing": False, "speed": 1}
    ax_play = fig.add_axes([0.03, 0.03, 0.09, 0.045])
    ax_spd = fig.add_axes([0.88, 0.03, 0.09, 0.045])
    b_play = Button(ax_play, "Play")
    b_spd = Button(ax_spd, "1x")
    timer = fig.canvas.new_timer(interval=int(base_ms))

    def step():
        slider.set_val((int(slider.val) + 1) % n)

    timer.add_callback(step)

    def toggle(_ev):
        if state["playing"]:
            timer.stop()
            b_play.label.set_text("Play")
        else:
            timer.start()
            b_play.label.set_text("Pause")
        state["playing"] = not state["playing"]

    def cycle(_ev):
        state["speed"] = {1: 2, 2: 4, 4: 1}[state["speed"]]
        b_spd.label.set_text(f"{state['speed']}x")
        timer.interval = int(base_ms / state["speed"])
        if state["playing"]:
            timer.stop()
            timer.start()

    b_play.on_clicked(toggle)
    b_spd.on_clicked(cycle)
    fig._player = (b_play, b_spd, timer)  # keep references alive


def animate(session: SessionData, save: str | None = None, fps: int = 8):
    import matplotlib.pyplot as plt
    from matplotlib.widgets import Slider

    cmap = tyre_cmap(session.opt_lo, session.opt_hi)
    vmin = scale_lo(session.opt_lo, session.opt_hi)
    vmax = scale_hi(session.opt_lo, session.opt_hi)
    n = len(session.grids)
    base_ms = 1000.0 / max(1, session.sample_rate_hz)

    fig, ax = plt.subplots(figsize=(6, 4.5))
    fig.subplots_adjust(bottom=0.26)
    im = ax.imshow(
        session.grids[0],
        cmap=cmap,
        vmin=vmin,
        vmax=vmax,
        aspect="auto",
        interpolation="nearest",
    )
    fig.colorbar(im, ax=ax, label="°C")
    cols = session.grids.shape[2]
    ax.set_xticks(np.arange(cols))
    ax.set_yticks([])
    ax.tick_params(length=0, labelsize=9)

    def update(idx):
        i = int(idx)
        im.set_data(session.grids[i])
        ax.set_xticklabels([f"{v:.0f}" for v in session.grids[i].mean(axis=0)])
        t = session.t_offsets_ms[i] / 1000.0
        ax.set_title(f"Wheel {session.wheel}   t = {t:.1f} s")
        fig.canvas.draw_idle()

    update(0)
    if save:
        import matplotlib.animation as animation

        anim = animation.FuncAnimation(fig, update, frames=n, interval=1000 / fps)
        anim.save(save, fps=fps)
        print(f"saved {save}")
        return anim
    sax = fig.add_axes([0.2, 0.10, 0.6, 0.03])
    fig._slider = Slider(sax, "time", 0, n - 1, valinit=0, valstep=1)
    fig._slider.on_changed(update)
    _add_player(fig, fig._slider, n, base_ms)
    plt.show()


def dashboard(sessions: list[SessionData], save: str | None = None, fps: int = 8):
    """2x2 FL/FR/RL/RR view grouped by shared session_id, with playback."""
    import matplotlib.pyplot as plt
    from matplotlib.widgets import Slider

    opt_lo, opt_hi = sessions[0].opt_lo, sessions[0].opt_hi
    cmap = tyre_cmap(opt_lo, opt_hi)
    layout = {"FL": (0, 0), "FR": (0, 1), "RL": (1, 0), "RR": (1, 1)}
    by_wheel = {s.wheel: s for s in sessions}
    n = min(len(s.grids) for s in sessions)
    vmin = scale_lo(opt_lo, opt_hi)
    vmax = scale_hi(opt_lo, opt_hi)
    base_ms = 1000.0 / max(1, sessions[0].sample_rate_hz)

    fig, axes = plt.subplots(2, 2, figsize=(9, 7))
    fig.subplots_adjust(bottom=0.16, hspace=0.5, wspace=0.25)
    ims = {}
    for wheel, (r, c) in layout.items():
        ax = axes[r][c]
        ax.set_title(wheel)
        ax.set_yticks([])
        s = by_wheel.get(wheel)
        if s is None:
            ax.set_xticks([])
            ax.set_axis_off()
            continue
        ax.set_xticks(np.arange(s.grids.shape[2]))
        ax.tick_params(length=0, labelsize=7)
        ims[wheel] = ax.imshow(
            s.grids[0],
            cmap=cmap,
            vmin=vmin,
            vmax=vmax,
            aspect="auto",
            interpolation="nearest",
        )

    fig.colorbar(
        next(iter(ims.values())), ax=axes.ravel().tolist(), fraction=0.025, label="°C"
    )

    def update(idx):
        i = int(idx)
        for wheel, im in ims.items():
            s = by_wheel[wheel]
            im.set_data(s.grids[i])
            ax = axes[layout[wheel][0]][layout[wheel][1]]
            ax.set_xticklabels([f"{v:.0f}" for v in s.grids[i].mean(axis=0)])
        t = by_wheel[next(iter(ims))].t_offsets_ms[i] / 1000.0
        fig.suptitle(f"t = {t:.1f} s")
        fig.canvas.draw_idle()

    update(0)
    if save:
        import matplotlib.animation as animation

        anim = animation.FuncAnimation(fig, update, frames=n, interval=1000 / fps)
        anim.save(save, fps=fps)
        print(f"saved {save}")
        return anim
    sax = fig.add_axes([0.2, 0.06, 0.6, 0.025])
    fig._slider = Slider(sax, "time", 0, n - 1, valinit=0, valstep=1)
    fig._slider.on_changed(update)
    _add_player(fig, fig._slider, n, base_ms)
    plt.show()
