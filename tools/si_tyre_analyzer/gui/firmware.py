"""Device firmware update: fetch the latest .bin from GitHub and push to a car.

Cascade (`/fw-upload`) stores the .bin on the master and tells paired wheels to
pull + flash it; the master must stay up to serve /fw.bin, so flashing the
master itself (`/update`, which reboots) is a separate, last step.
"""

from __future__ import annotations

import os
import tempfile
import urllib.request
from dataclasses import dataclass

from . import update

FW_TAG_PREFIX = "si-tyre-analyzer-v"


@dataclass
class FwRelease:
    """A published firmware release: version, .bin download URL, and notes."""

    version: str
    bin_url: str
    notes: str


def latest() -> FwRelease | None:
    best: FwRelease | None = None
    for rel in update._fetch():
        tag = rel.get("tag_name", "")
        if (
            not tag.startswith(FW_TAG_PREFIX)
            or rel.get("draft")
            or rel.get("prerelease")
        ):
            continue
        ver = tag[len(FW_TAG_PREFIX) :]
        binurl = next(
            (
                a["browser_download_url"]
                for a in rel.get("assets", [])
                if a.get("name", "").endswith(".bin")
            ),
            None,
        )
        if not binurl:
            continue
        if best is None or update._parse(ver) > update._parse(best.version):
            best = FwRelease(ver, binurl, rel.get("body", "") or "")
    return best


def device_version(host: str) -> str:
    import requests

    r = requests.get(f"http://{host}/api/peers", timeout=10)
    r.raise_for_status()
    return r.json().get("master_fw", "")


def check(host: str) -> FwRelease | None:
    """Latest firmware release newer than the connected master, else None."""
    best = latest()
    if best is None:
        return None
    if update._parse(best.version) > update._parse(device_version(host)):
        return best
    return None


def _download(url: str) -> str:
    tmp = tempfile.mkdtemp(prefix="sita-fw-")
    dest = os.path.join(tmp, url.rsplit("/", 1)[-1])
    urllib.request.urlretrieve(url, dest)
    return dest


def _post(host: str, path: str, bin_path: str) -> None:
    import requests

    with open(bin_path, "rb") as f:
        r = requests.post(
            f"http://{host}{path}",
            files={
                "firmware": (os.path.basename(bin_path), f, "application/octet-stream")
            },
            timeout=180,
        )
    r.raise_for_status()


def cascade(host: str, release: FwRelease) -> None:
    """Push to the master, which propagates to all paired wheels."""
    _post(host, "/fw-upload", _download(release.bin_url))


def flash_master(host: str, release: FwRelease) -> None:
    """Flash the connected master itself; it reboots and drops the AP."""
    _post(host, "/update", _download(release.bin_url))
