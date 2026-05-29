<p align="center">
  <img src="tools/si_tyre_analyzer/gui/assets/si_tyre_logo.svg" alt="SI Tyre Analyzer" width="440">
</p>

ESP32-S3 firmware that logs car **tyre temperatures** during a track session with
an Adafruit **MLX90640** (32×24 thermal array, one per wheel), plus the **SI Tyre
Analyzer** desktop app to view and analyse the data.

## How it works

- 4 **wheel units** (ESP32-S3 + MLX90640) each downsample the 32×24 frame into a
  9 × 5 grid (tread width × rows) and log it to internal flash (LittleFS).
- 1 **dash master** (sensorless ESP32-S3) coordinates start/stop over ESP-NOW and
  serves a live 4-wheel dashboard.
- Wheel units stream a live grid to the master during a run and upload their full
  recording to it afterwards; download and analyse runs in the desktop app.

## One firmware for all 5 devices

There is a **single firmware**, `firmware_si_tyre_analyzer_v<ver>.bin`. Role
(master/slave), wheel (FL/FR/RL/RR) and `has_sensor` are runtime config stored in
NVS and set from the web UI — flash the _same_ `.bin` to every device and
configure each one afterwards. The `custom_sensor_type` build flag (`mock` /
`mlx90640`) selects the sensor driver.

## Build & flash (PlatformIO)

```
pio run -e esp32-s3-devkitc-1            # default: mock sensor
pio run -e esp32-s3-devkitc-1 -t upload  # flash the same bin to every device
```

For the real sensor, set `custom_sensor_type = mlx90640` in `platformio.ini`. The
`custom_*` role/wheel values only seed first-boot NVS; change them per device in
the web UI.

### Controls (GPIO0 button)

- **tap** — start / stop a recording session
- **hold ~3 s** — enter ESP-NOW pairing
- **hold ~8 s** — enter WiFi AP / config mode (SSID `SITA <wheel> <car-or-id>`; serves
  config, session downloads, and firmware `.bin` upload / OTA)

The status LED is solid while recording, blinks in AP mode, and mirrors the
button gesture stage while held.

### Pairing

Set the same **Car ID** on every device in a car (web UI), then press **Pair
wheels** on the master and the wheels join. Different Car IDs keep neighbouring
cars from cross-pairing. Update all wheels at once with **Upload & flash wheels**
on the master.

## SI Tyre Analyzer (desktop app)

A cross-platform desktop app (PySide6) under `tools/`. Pages:

- **Live** — connect the laptop to the master's WiFi AP, enter its host
  (`192.168.4.1`), and watch the 4-wheel heatmap live.
- **Sessions** — list + download runs from a device over WiFi into a local
  library, grouped by run.
- **Viewer** — open a run and play/scrub the 4-wheel heatmaps (slider + speed).
- **Analysis** — tread-profile bands (inner/middle/outer) vs time per wheel.

### One-time setup (no manual package installs)

1. Install **Python 3.10+** — https://www.python.org/downloads/ (on Windows tick
   "Add Python to PATH").
2. Install **uv** (single binary, manages the venv + dependencies):
   - Windows (PowerShell): `irm https://astral.sh/uv/install.ps1 | iex`
   - macOS / Linux: `curl -LsSf https://astral.sh/uv/install.sh | sh`

`uv` fetches numpy/matplotlib/PySide6/requests automatically on first run from
`tools/pyproject.toml`.

### Run

```
cd tools
uv run si-tyre-analyzer
```

First launch downloads dependencies (a minute or two). Behind a TLS-intercepting
proxy add `--native-tls` (`uv run --native-tls si-tyre-analyzer`).

### CLI (optional, same data)

```
cd tools
uv run si-tyre info       <run>.bin
uv run si-tyre heatmap    <run>.bin --save out.mp4
uv run si-tyre dashboard  ~/SI_Tyre_Analyzer_Runs
uv run si-tyre fetch --host 192.168.4.1 --all --dest ~/SI_Tyre_Analyzer_Runs
```

## Development

- Native unit tests: `pio test -e native_downsample -e native_logformat -e native_version` (needs a host C++ compiler).
- Python contract test: `cd tools && uv run pytest`.
- Libraries are vendored under `lib/` (ArduinoJson; the `mlx90640` build also uses
  Adafruit MLX90640 + Adafruit Unified Sensor).
- `STATUS_LED_PIN` defaults to GPIO2; the ESP32-S3-DevKitC-1 onboard RGB LED is on
  GPIO48 — wire a plain LED or adjust for the RGB driver.
