"""Tyre heatmap colour map — blue->green->yellow->red ramp."""

from __future__ import annotations

_RAMP = (
    (0.00, 30, 70, 200),
    (0.20, 30, 165, 215),
    (0.40, 40, 185, 80),
    (0.62, 70, 200, 70),
    (0.72, 200, 210, 50),
    (0.85, 240, 150, 30),
    (1.00, 215, 30, 30),
)


def heat_rgb(t: float, lo: float, hi: float) -> tuple[int, int, int]:
    """Map temperature t in [lo,hi] to an (r,g,b) 0-255 tuple."""
    span = hi - lo
    f = 0.0 if span <= 0 else (t - lo) / span
    f = max(0.0, min(1.0, f))
    for i in range(len(_RAMP) - 1):
        a, b = _RAMP[i], _RAMP[i + 1]
        if f <= b[0]:
            u = (f - a[0]) / (b[0] - a[0])
            return (
                round(a[1] + (b[1] - a[1]) * u),
                round(a[2] + (b[2] - a[2]) * u),
                round(a[3] + (b[3] - a[3]) * u),
            )
    return _RAMP[-1][1], _RAMP[-1][2], _RAMP[-1][3]


def tyre_cmap():
    """matplotlib colormap matching heat_rgb (for embedded charts)."""
    from matplotlib.colors import ListedColormap

    cols = [tuple(v / 255 for v in heat_rgb(i / 255.0, 0.0, 1.0)) for i in range(256)]
    return ListedColormap(cols)
