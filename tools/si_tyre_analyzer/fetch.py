"""Fetch session files from a device over HTTP (AP mode, default 192.168.4.1)."""

from __future__ import annotations

import os


def list_sessions(host: str) -> list[dict]:
    import requests
    r = requests.get(f"http://{host}/api/sessions", timeout=10)
    r.raise_for_status()
    return r.json()


def download(host: str, name: str, dest_dir: str = ".") -> str:
    import requests
    os.makedirs(dest_dir, exist_ok=True)
    url = f"http://{host}/download?file={name}"
    dest = os.path.join(dest_dir, name)
    with requests.get(url, stream=True, timeout=30) as r:
        r.raise_for_status()
        with open(dest, "wb") as f:
            for chunk in r.iter_content(8192):
                f.write(chunk)
    return dest


def download_all(host: str, dest_dir: str = ".") -> list[str]:
    return [download(host, s["name"], dest_dir) for s in list_sessions(host)]
