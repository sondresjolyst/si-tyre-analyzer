"""Tyre heatmap colour map — same blue->warm->red ramp as the device /live."""

from __future__ import annotations


def heat_rgb(t: float, lo: float, hi: float) -> tuple[int, int, int]:
    """Map temperature t in [lo,hi] to an (r,g,b) 0-255 tuple."""
    span = hi - lo
    f = 0.0 if span <= 0 else (t - lo) / span
    f = max(0.0, min(1.0, f))
    r = round(255 * min(1.0, f * 2))
    b = round(255 * min(1.0, (1 - f) * 2))
    g = round(80 * (1 - abs(f - 0.5) * 2))
    return r, max(0, g), b


def tyre_cmap():
    """matplotlib colormap matching heat_rgb (for embedded charts)."""
    from matplotlib.colors import ListedColormap
    cols = [tuple(v / 255 for v in heat_rgb(i / 255.0, 0.0, 1.0))
            for i in range(256)]
    return ListedColormap(cols)
