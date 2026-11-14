<p align="center">
  <img src="tools/si_tyre_analyzer/gui/assets/si_tyre_logo.svg" alt="SI Tyre Analyzer" width="440">
</p>

<p align="center">
  Tyre temperature across the tread, on every wheel, throughout a track session.
</p>

---

SI Tyre Analyzer measures tyre temperature on all 4 wheels while you drive. A
thermal sensor on each wheel records the inner, middle and outer tread for the
whole session. The desktop app then shows the tread profile, the balance between
wheels, temperature over time, and how long each tyre stayed in its target
window.

## What it is

- **4 wheel units.** Each has an ESP32-S3 and an MLX90640 thermal sensor, and
  logs to its own memory.
- **1 dash master.** It has no sensor. It starts and stops all 4 wheels together
  and shows a live view of all 4.
- **SI Tyre Analyzer.** The desktop app for Windows, macOS and Linux. Use it to
  download and analyse runs.

---

## Quick start

### 1. Install the app

Run one command. It installs the `uv` runtime and the app. On Windows it also
adds a desktop shortcut.

- **Windows**, in PowerShell:
  ```powershell
  irm https://raw.githubusercontent.com/sondresjolyst/si-tyre-analyzer/main/tools/install/install.ps1 | iex
  ```
- **macOS and Linux**:
  ```bash
  curl -LsSf https://raw.githubusercontent.com/sondresjolyst/si-tyre-analyzer/main/tools/install/install.sh | sh
  ```

Open **SI Tyre Analyzer** from the desktop shortcut, or run `si-tyre-analyzer`
in a terminal.

### 2. Pair a car once

1. Turn on all units.
2. Hold the master's button until the LED blinks fast. This takes about 3 s.
   Then let go. The master looks for wheels for 60 s, and its LED keeps
   blinking fast.
3. Within those 60 s, do the same on each wheel unit. Keep the wheel unit
   close to the master.
4. Look at the wheel's LED. It stops blinking fast as soon as the wheel is
   paired. If it blinks for the full 60 s, the wheel did not find the master.
   Move it closer and try again.

Do not hold the button until the LED stays on. The unit then starts config mode
when you let go. A short press stops pairing early.

The pairing stays after you turn the units off. The master's web page shows how
many wheels are paired.

If another car is pairing nearby, enter the master's Car ID on each wheel first.
Find the Car ID on the master's web page. Enter it in the Car ID field on the
wheel's web page. The wheel then pairs only with that master. It also looks for
the master by itself each time you turn it on, until it is paired.

### 3. Record a session

- Press the master's button once to start. Press it again to stop. All 4 wheels
  record together on the same clock.
- You can also select “● Start recording” on the master's web page. The button
  then changes to “■ Stop recording”.
- The status LED stays on while the unit records.
- Recording stops on its own after 30 minutes.

### 4. Download and analyse

1. Connect your laptop to the master's Wi-Fi.
2. In the app, go to **Sessions** and select “Show device sessions”.
3. Download the run. See [Downloading runs](#downloading-runs).
4. Open the run in **Viewer** or **Analysis**.

---

## The app

| Page         | What it does                                                                                                            |
| ------------ | ----------------------------------------------------------------------------------------------------------------------- |
| **Sessions** | Lists runs on a connected device and downloads them to your local library. Configures a device or updates its firmware. |
| **Live**     | Connects to the master at `192.168.4.1` and shows all 4 wheels live.                                                    |
| **Viewer**   | Replays a run. Scrub through or play the 4 heatmaps at the speed you choose.                                            |
| **Analysis** | Tread profile for inner, middle and outer, temperature over time, time in window, and front/rear and left/right balance. |

### Downloading runs

On the **Sessions** page, select “Show device sessions” first. Then:

- To download one run, double-click it. Or select it, then select “Download…” >
  “Download selected”.
- To download every run on the device, select “Download…” > “Download all”.

The app saves runs in your local library and groups them by session.

### Configuring a device

On **Sessions**, select “Configure device…”. This opens the device's web page.
There you set the role, wheel, Car ID and sensor options.

The **Optimal window** on the master sets the target tread temperature, as a low
and a high value in °C. It is the green zone on the heatmaps. The time in window
analysis counts the time spent inside it. See
[docs/colour-scale.md](docs/colour-scale.md).

---

## Keeping things up to date

The app checks for a new version when it starts and asks before it installs it.
You can also select “Check for updates”. You update the firmware on the car over
Wi-Fi from the app. See [docs/updating.md](docs/updating.md).

---

## The device, in detail

For mounting and aiming the sensors, see [docs/mounting.md](docs/mounting.md).

**One firmware for every unit.** All 5 units run the same
`firmware_si_tyre_analyzer_v<ver>.bin`. Each device stores its role, wheel
position and whether it has a sensor. You change these on the device web page.
You never flash a unit with its own build.

**Button.** One button does 3 things, by how long you hold it.

| Gesture     | Action                                                      |
| ----------- | ----------------------------------------------------------- |
| Short press | Start or stop recording. The master controls the whole car. |
| Hold ~3 s   | Start pairing                                               |
| Hold ~8 s   | Start Wi-Fi config mode                                     |

**Status LED.**

| LED                   | Meaning                                               |
| --------------------- | ----------------------------------------------------- |
| On                    | Recording, or button held long enough for config mode |
| Fast blink            | Pairing, or button held long enough for pairing       |
| Blink                 | Config mode                                           |
| Short flash every 2 s | Idle                                                  |

**Config mode.** The device starts a Wi-Fi network named `SITA <wheel> <car name>`.
Connect to it, and the device web page opens on its own. If it does not, go to
`http://192.168.4.1`. On the page you can change settings, download sessions and
upload a firmware `.bin` by hand. Everything the app does also works here from a
phone or laptop browser.

If no car name is set, the network name uses the chip ID instead.

---

## For developers

<details>
<summary>Build, flash, and test from source</summary>

### Build and flash with PlatformIO

```bash
pio run -e esp32-s3-devkitc-1            # default: mock sensor
pio run -e esp32-s3-devkitc-1 -t upload  # flash the same bin to every unit
```

For the real sensor, set `custom_sensor_type = mlx90640` in `platformio.ini`.
The `custom_*` role and wheel values only set the config on first boot. Change
them per device in the web UI.

### Run the app from source

1. Install **Python 3.11+** and **uv**. On Windows, select "Add Python to PATH"
   in the Python installer.
   - Windows: `irm https://astral.sh/uv/install.ps1 | iex`
   - macOS and Linux: `curl -LsSf https://astral.sh/uv/install.sh | sh`
2. Run the app:
   ```bash
   cd tools
   uv run si-tyre-analyzer
   ```

Behind a TLS-intercepting proxy, add `--native-tls`.

### CLI

```bash
cd tools
uv run sita info       <run>.bin
uv run sita heatmap    <run>.bin --save out.mp4
uv run sita dashboard  ~/Documents/SI\ Tyre\ Analyzer
uv run sita fetch --host 192.168.4.1 --all --dest ~/Documents/SI\ Tyre\ Analyzer
```

### Testing

See [docs/testing.md](docs/testing.md). It covers mocks for testing without
hardware, and unit and contract tests.

### Notes

- Each wheel unit reduces the MLX90640's 32×24 frame to a 9×5 grid and logs it
  to LittleFS. The grid is tread width by rows.
- The master coordinates the wheels over **ESP-NOW**. It serves the live
  dashboard over its own Wi-Fi access point. The device never joins a router.
- Libraries are vendored under `lib/`. The build uses ArduinoJson. The
  `mlx90640` build also uses Adafruit MLX90640 and Adafruit Unified Sensor.
- For the optional dashboard screen, see [docs/dash-gauge.md](docs/dash-gauge.md).

</details>
