"""Render the app mark SVG to a Windows .ico (multi-size, PNG-backed)."""

from __future__ import annotations

import struct
from pathlib import Path

ASSETS = Path(__file__).parent / "assets"
MARK = ASSETS / "si_tyre_mark.svg"
SIZES = (16, 24, 32, 48, 64, 128, 256)


def _png(svg: str, size: int) -> bytes:
    from PySide6.QtCore import QBuffer, QByteArray, QRectF
    from PySide6.QtGui import QImage, QPainter
    from PySide6.QtSvg import QSvgRenderer

    img = QImage(size, size, QImage.Format_ARGB32)
    img.fill(0)
    painter = QPainter(img)
    QSvgRenderer(svg).render(painter, QRectF(0, 0, size, size))
    painter.end()
    buf = QByteArray()
    dev = QBuffer(buf)
    dev.open(QBuffer.WriteOnly)
    img.save(dev, "PNG")
    return bytes(buf)


def render_ico(dest: str, svg: str | None = None, sizes=SIZES) -> str:
    """Write a PNG-backed .ico of the mark to *dest*; return the path."""
    from PySide6.QtGui import QGuiApplication

    if QGuiApplication.instance() is None:
        QGuiApplication([])
    svg = svg or str(MARK)
    pngs = [_png(svg, s) for s in sizes]

    offset = 6 + 16 * len(pngs)
    head = struct.pack("<HHH", 0, 1, len(pngs))
    entries, blob = b"", b""
    for size, png in zip(sizes, pngs):
        entries += struct.pack(
            "<BBBBHHII", size & 0xFF, size & 0xFF, 0, 0, 1, 32, len(png), offset
        )
        offset += len(png)
        blob += png

    Path(dest).parent.mkdir(parents=True, exist_ok=True)
    Path(dest).write_bytes(head + entries + blob)
    return dest
