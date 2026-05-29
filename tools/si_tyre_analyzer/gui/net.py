"""Background networking threads for the GUI."""

from __future__ import annotations

import time

from PySide6.QtCore import QThread, Signal


class LivePoller(QThread):
    """Polls GET http://<host>/api/live every ~0.5s until stopped."""

    data = Signal(dict)
    error = Signal(str)

    def __init__(self, host: str):
        super().__init__()
        self._host = host
        self._run = True

    def stop(self):
        self._run = False

    def run(self):
        import requests
        while self._run:
            try:
                r = requests.get(f"http://{self._host}/api/live", timeout=2)
                r.raise_for_status()
                self.data.emit(r.json())
            except Exception as e:
                self.error.emit(str(e))
            for _ in range(5):
                if not self._run:
                    break
                time.sleep(0.1)


class Worker(QThread):
    """Runs a callable off the UI thread; emits done(result) or failed(msg)."""

    done = Signal(object)
    failed = Signal(str)

    def __init__(self, fn):
        super().__init__()
        self._fn = fn

    def run(self):
        try:
            self.done.emit(self._fn())
        except Exception as e:
            self.failed.emit(str(e))
