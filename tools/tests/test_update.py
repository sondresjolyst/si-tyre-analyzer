"""Update / version logic — pure, network-free (monkeypatches the releases fetch).

Guards the self-update path a non-technical customer relies on: semver ordering,
release filtering, asset selection, and the firmware/app tag-prefix split.
"""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from si_tyre_analyzer.gui import firmware, update


def _rel(tag, assets, draft=False, prerelease=False, body=""):
    return {
        "tag_name": tag,
        "draft": draft,
        "prerelease": prerelease,
        "body": body,
        "assets": [
            {"name": n, "browser_download_url": f"https://x/{n}"} for n in assets
        ],
    }


# ---- semver parsing ----
def test_parse_numeric_not_lexical():
    assert update._parse("0.10.0") > update._parse("0.9.0")


def test_parse_strips_prefix_and_suffix():
    assert update._parse("v1.2.3") == (1, 2, 3)
    assert update._parse("1.2.3-rc1") == (1, 2, 3)
    assert update._parse("1.2.3+build9") == (1, 2, 3)


# ---- app update check ----
def _patch_app(monkeypatch, releases, current):
    monkeypatch.setattr(update, "_fetch", lambda *a, **k: releases)
    monkeypatch.setattr(update, "current_version", lambda: current)


def test_check_returns_newest_app_wheel(monkeypatch):
    _patch_app(
        monkeypatch,
        [
            _rel("si-tyre-analyzer-app-v0.2.0", ["si_tyre_analyzer-0.2.0.whl"]),
            _rel("si-tyre-analyzer-app-v0.1.0", ["si_tyre_analyzer-0.1.0.whl"]),
            _rel("si-tyre-analyzer-v9.9.9", ["firmware.bin"]),  # firmware tag ignored
        ],
        current="0.1.0",
    )
    rel = update.check()
    assert rel is not None
    assert rel.version == "0.2.0"
    assert rel.wheel_url.endswith("0.2.0.whl")


def test_check_orders_numerically(monkeypatch):
    _patch_app(
        monkeypatch,
        [
            _rel("si-tyre-analyzer-app-v0.9.0", ["a-0.9.0.whl"]),
            _rel("si-tyre-analyzer-app-v0.10.0", ["a-0.10.0.whl"]),
        ],
        current="0.1.0",
    )
    assert update.check().version == "0.10.0"


def test_check_skips_draft_and_prerelease(monkeypatch):
    _patch_app(
        monkeypatch,
        [
            _rel("si-tyre-analyzer-app-v0.3.0", ["a-0.3.0.whl"], draft=True),
            _rel("si-tyre-analyzer-app-v0.4.0", ["a-0.4.0.whl"], prerelease=True),
            _rel("si-tyre-analyzer-app-v0.2.0", ["a-0.2.0.whl"]),
        ],
        current="0.1.0",
    )
    assert update.check().version == "0.2.0"


def test_check_skips_release_without_wheel(monkeypatch):
    _patch_app(
        monkeypatch,
        [
            _rel("si-tyre-analyzer-app-v0.5.0", ["notes.txt", "firmware.bin"]),
        ],
        current="0.1.0",
    )
    assert update.check() is None


def test_check_none_when_current_is_latest(monkeypatch):
    _patch_app(
        monkeypatch,
        [
            _rel("si-tyre-analyzer-app-v0.2.0", ["a-0.2.0.whl"]),
        ],
        current="0.2.0",
    )
    assert update.check() is None


# ---- firmware update check ----
def test_firmware_prefix_does_not_match_app_tag(monkeypatch):
    monkeypatch.setattr(
        update,
        "_fetch",
        lambda *a, **k: [
            _rel("si-tyre-analyzer-app-v0.9.0", ["a-0.9.0.whl"]),
        ],
    )
    assert firmware.latest() is None


def test_firmware_latest_picks_newest_bin(monkeypatch):
    monkeypatch.setattr(
        update,
        "_fetch",
        lambda *a, **k: [
            _rel("si-tyre-analyzer-v0.1.0", ["firmware-0.1.0.bin"]),
            _rel("si-tyre-analyzer-v0.2.0", ["firmware-0.2.0.bin"]),
            _rel("si-tyre-analyzer-app-v9.9.9", ["app.whl"]),  # app tag ignored
        ],
    )
    rel = firmware.latest()
    assert rel is not None and rel.version == "0.2.0"
    assert rel.bin_url.endswith("0.2.0.bin")


def test_firmware_check_compares_device(monkeypatch):
    monkeypatch.setattr(
        update,
        "_fetch",
        lambda *a, **k: [
            _rel("si-tyre-analyzer-v0.2.0", ["firmware-0.2.0.bin"]),
        ],
    )
    monkeypatch.setattr(firmware, "device_version", lambda host: "v0.1.0")
    assert firmware.check("1.2.3.4").version == "0.2.0"
    monkeypatch.setattr(firmware, "device_version", lambda host: "v0.2.0")
    assert firmware.check("1.2.3.4") is None


if __name__ == "__main__":
    import pytest

    raise SystemExit(pytest.main([__file__, "-q"]))
