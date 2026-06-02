"""Build a minimal unsigned macOS .app wrapping the installed GUI launcher."""

from __future__ import annotations

import plistlib
import shutil
import subprocess
import tempfile
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path

from .iconize import MARK, _png

APP_NAME = "SI Tyre Analyzer"
BUNDLE_ID = "com.sondresjolyst.si-tyre-analyzer"
EXEC_NAME = "si-tyre-analyzer"
ICNS_SIZES = (16, 32, 128, 256, 512)


def _version() -> str:
    try:
        return version("si-tyre-analyzer")
    except PackageNotFoundError:
        return "0.0.0"


def _build_icns(dest: Path) -> bool:
    """Render the mark SVG to a multi-resolution .icns via iconutil."""
    from PySide6.QtGui import QGuiApplication

    if QGuiApplication.instance() is None:
        QGuiApplication([])
    with tempfile.TemporaryDirectory() as tmp:
        iconset = Path(tmp) / "AppIcon.iconset"
        iconset.mkdir()
        for size in ICNS_SIZES:
            for scale in (1, 2):
                suffix = "@2x" if scale == 2 else ""
                (iconset / f"icon_{size}x{size}{suffix}.png").write_bytes(
                    _png(str(MARK), size * scale)
                )
        subprocess.run(
            ["iconutil", "-c", "icns", "-o", str(dest), str(iconset)], check=True
        )
    return True


def build_app(dest_dir: str | Path | None = None, launcher: str | None = None) -> str:
    """Write ``SI Tyre Analyzer.app`` into *dest_dir* (default ~/Applications).

    *launcher* is the GUI entry point to exec; resolved from PATH if omitted.
    Returns the bundle path.
    """
    launcher = launcher or shutil.which(EXEC_NAME)
    if not launcher:
        raise SystemExit(f"{EXEC_NAME} not on PATH — install the app first.")

    dest_dir = Path(dest_dir).expanduser() if dest_dir else Path.home() / "Applications"
    app_dir = dest_dir / f"{APP_NAME}.app"
    contents = app_dir / "Contents"
    macos = contents / "MacOS"
    resources = contents / "Resources"
    if app_dir.exists():
        shutil.rmtree(app_dir)
    macos.mkdir(parents=True)
    resources.mkdir(parents=True)

    launch = macos / EXEC_NAME
    launch.write_text(f'#!/bin/bash\nexec "{launcher}" "$@"\n')
    launch.chmod(0o755)

    info: dict[str, object] = {
        "CFBundleName": APP_NAME,
        "CFBundleDisplayName": APP_NAME,
        "CFBundleExecutable": EXEC_NAME,
        "CFBundleIdentifier": BUNDLE_ID,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": _version(),
        "CFBundleVersion": _version(),
        "LSMinimumSystemVersion": "11.0",
        "NSHighResolutionCapable": True,
    }
    try:
        _build_icns(resources / "AppIcon.icns")
        info["CFBundleIconFile"] = "AppIcon"
    except Exception as e:  # noqa: BLE001 — icon is best-effort
        print(f"icon skipped: {e}")

    (contents / "Info.plist").write_bytes(plistlib.dumps(info))
    return str(app_dir)
