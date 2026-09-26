"""Logo wordmark."""

import os

ASSETS = os.path.join(os.path.dirname(__file__), "..", "si_tyre_analyzer", "gui", "assets")


def test_wordmark_names_the_registered_company():
    with open(os.path.join(ASSETS, "si_tyre_logo.svg"), encoding="utf-8") as f:
        svg = f.read()
    assert "SJ&#216;LYST INNOVATION AS" in svg
    assert "INNOVATIONS" not in svg
