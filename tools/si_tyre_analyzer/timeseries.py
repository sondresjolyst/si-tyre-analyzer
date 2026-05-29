"""Per-zone temperature-over-time charts."""

from __future__ import annotations

import numpy as np

from .logreader import SessionData


def plot(session: SessionData, save: str | None = None):
    import matplotlib.pyplot as plt

    t = session.t_offsets_ms / 1000.0
    # Aggregate rows into per-column (tread) bands; tread width is most relevant.
    by_col = session.grids.mean(axis=1)  # (N, cols)

    fig, ax = plt.subplots(figsize=(8, 4))
    for c in range(session.cols):
        ax.plot(t, by_col[:, c], label=f"col {c}")
    ax.set_xlabel("time (s)")
    ax.set_ylabel("°C")
    ax.set_title(f"Wheel {session.wheel} — tread columns (inner → outer)")
    ax.legend(ncol=3, fontsize=8)
    ax.grid(True, alpha=0.3)

    if save:
        fig.savefig(save, dpi=120)
        print(f"saved {save}")
    else:
        plt.show()
