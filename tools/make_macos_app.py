#!/usr/bin/env python3
"""Dev helper: build the macOS .app from the current venv.

Same as `sita install-app`; kept for the dev workflow.
    uv run python tools/make_macos_app.py [dest_dir]
"""

from __future__ import annotations

import sys

from si_tyre_analyzer.gui.macapp import build_app

if __name__ == "__main__":
    dest = sys.argv[1] if len(sys.argv) > 1 else None
    print(build_app(dest))
