"""In-app update check against GitHub Releases, applied via `uv tool install`."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from dataclasses import dataclass
from pathlib import Path

REPO = "sondresjolyst/si-tyre-analyzer"
TAG_PREFIX = "si-tyre-analyzer-app-v"
DIST_NAME = "si-tyre-analyzer"
API = f"https://api.github.com/repos/{REPO}/releases"


def current_version() -> str:
    try:
        from importlib.metadata import version

        return version(DIST_NAME)
    except Exception:
        pass
    try:
        import tomllib

        pyproject = Path(__file__).resolve().parents[2] / "pyproject.toml"
        with open(pyproject, "rb") as f:
            return tomllib.load(f)["project"]["version"]
    except Exception:
        return "0.0.0"


def _parse(v: str) -> tuple[int, ...]:
    core = v.lstrip("v").split("-", 1)[0].split("+", 1)[0]
    out = []
    for part in core.split("."):
        try:
            out.append(int(part))
        except ValueError:
            out.append(0)
    return tuple(out)


@dataclass
class Release:
    """A published app release: version, wheel download URL, and notes."""

    version: str
    wheel_url: str
    notes: str


def _fetch(timeout: float = 6.0) -> list:
    req = urllib.request.Request(
        API,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": DIST_NAME,
        },
    )
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.load(r)


def check() -> Release | None:
    """Newest published release newer than the running version, else None."""
    best: Release | None = None
    for rel in _fetch():
        tag = rel.get("tag_name", "")
        if not tag.startswith(TAG_PREFIX) or rel.get("draft") or rel.get("prerelease"):
            continue
        ver = tag[len(TAG_PREFIX) :]
        wheel = next(
            (
                a["browser_download_url"]
                for a in rel.get("assets", [])
                if a.get("name", "").endswith(".whl")
            ),
            None,
        )
        if not wheel:
            continue
        if best is None or _parse(ver) > _parse(best.version):
            best = Release(ver, wheel, rel.get("body", "") or "")
    if best and _parse(best.version) > _parse(current_version()):
        return best
    return None


def apply(release: Release) -> None:
    """Download the wheel and hand off install to a detached helper.

    The running launcher locks its own exe on Windows, so uv cannot replace
    the entrypoint while the app is open. The helper waits for this process to
    exit, runs ``uv tool install``, then relaunches the app.
    """
    uv = shutil.which("uv")
    if not uv:
        raise RuntimeError("uv was not found on PATH; cannot self-update.")
    tmp = tempfile.mkdtemp(prefix="sita-update-")
    dest = os.path.join(tmp, release.wheel_url.rsplit("/", 1)[-1])
    urllib.request.urlretrieve(release.wheel_url, dest)
    _spawn_updater(uv, dest, tmp)


def _spawn_updater(uv: str, wheel: str, tmp: str) -> None:
    pid = os.getpid()
    exe = shutil.which(DIST_NAME) or sys.argv[0]
    log = os.path.join(tmp, "update.log")

    if os.name == "nt":
        script = os.path.join(tmp, "update.ps1")
        Path(script).write_text(
            f"try {{ Wait-Process -Id {pid} -ErrorAction SilentlyContinue }} "
            f"catch {{}}\n"
            f"& '{uv}' tool install --force '{wheel}' *> '{log}'\n"
            f"Start-Process '{exe}'\n",
            encoding="utf-8",
        )
        subprocess.Popen(
            [
                "powershell",
                "-NoProfile",
                "-WindowStyle",
                "Hidden",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                script,
            ],
            creationflags=0x00000008 | 0x00000200,  # DETACHED | NEW_PROCESS_GROUP
            close_fds=True,
        )
    else:
        script = (
            f"while kill -0 {pid} 2>/dev/null; do sleep 0.3; done\n"
            f"'{uv}' tool install --force '{wheel}' >'{log}' 2>&1\n"
            f"'{exe}' &\n"
        )
        subprocess.Popen(["sh", "-c", script], start_new_session=True, close_fds=True)
