"""Tyre heatmap colour map — blue->green->yellow->red ramp.

The optimal window drives the whole scale: cold 40%, optimal (green) 30%,
high 15%, superhigh 15%. Mirrors src/display/Colormap.h.
"""

from __future__ import annotations

_COLORS = (
    (30, 70, 200),
    (30, 165, 215),
    (40, 185, 80),
    (70, 200, 70),
    (200, 210, 50),
    (240, 150, 30),
    (215, 30, 30),
)


def scale_lo(opt_lo: float, opt_hi: float) -> float:
    return opt_lo - 1.33333 * (opt_hi - opt_lo)


def scale_hi(opt_lo: float, opt_hi: float) -> float:
    return opt_hi + (opt_hi - opt_lo)


def heat_rgb(t: float, opt_lo: float, opt_hi: float) -> tuple[int, int, int]:
    """Map temperature t to an (r,g,b) 0-255 tuple for the given window."""
    g = opt_hi - opt_lo
    if g <= 0:
        g = 1.0
    stops = (
        opt_lo - 1.33333 * g,
        opt_lo - 0.66667 * g,
        opt_lo,
        opt_lo + 0.6875 * g,
        opt_hi,
        opt_hi + 0.5 * g,
        opt_hi + g,
    )
    if t <= stops[0]:
        return _COLORS[0]
    for i in range(len(stops) - 1):
        if t <= stops[i + 1]:
            a, b = _COLORS[i], _COLORS[i + 1]
            span = stops[i + 1] - stops[i]
            u = 0.0 if span <= 0 else (t - stops[i]) / span
            return (
                round(a[0] + (b[0] - a[0]) * u),
                round(a[1] + (b[1] - a[1]) * u),
                round(a[2] + (b[2] - a[2]) * u),
            )
    return _COLORS[-1]


def tyre_cmap(opt_lo: float, opt_hi: float):
    """matplotlib colormap matching heat_rgb (for embedded charts)."""
    from matplotlib.colors import ListedColormap

    lo, hi = scale_lo(opt_lo, opt_hi), scale_hi(opt_lo, opt_hi)
    cols = [
        tuple(v / 255 for v in heat_rgb(lo + (hi - lo) * i / 255.0, opt_lo, opt_hi))
        for i in range(256)
    ]
    return ListedColormap(cols)
