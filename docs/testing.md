# Testing

## Without hardware

Two mocks let you exercise the firmware and web UI with no MLX90640 attached:

- **On-device mock sensor** — the default env builds with
  `custom_sensor_type = mock`, so the firmware synthesises frames (warm-up
  curve, a roaming hot spot, parabolic tread falloff, per-pixel noise). Flash
  it, enable **Has sensor** in the web config, then open `/align` or `/live`
  for live synthetic heatmaps. Recording works too.
- **PC live server** — `tools/mock_live_server.py` serves an animated
  `/api/live` (4 wheels) on `http://127.0.0.1:8080`, for testing a Live viewer
  with no device at all:
  ```bash
  python tools/mock_live_server.py
  ```

## Automated tests

- Native unit tests: `pio test -e native_downsample -e native_logformat -e native_version`
  (needs a host C++ compiler).
- Python contract test: `cd tools && uv run pytest`.
