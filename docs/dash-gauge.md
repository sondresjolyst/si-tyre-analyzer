# Dash gauge (ILI9341 TFT)

An optional colour screen on the dash master shows live tyre temperatures: a 2×2
grid of per-wheel heatmaps with inner / middle / outer °C, fed by the live grids
in `LiveDashboard`. The panel is a 2.8″ ILI9341 SPI TFT (320×240) with XPT2046
touch.

The driver ships in the standard firmware. A unit drives a screen only when it is
a **master** with **Dash display fitted?** set on.

## Enabling

On the master's web page, set **Role = Dash master** and **Dash display fitted? =
Yes**, save, and reboot.

## Pinout (S3-MINI master → ILI9341 + XPT2046)

| Signal | GPIO | Signal    | GPIO      |
| ------ | ---- | --------- | --------- |
| SCK    | 4    | RST       | 15        |
| MOSI   | 5    | T_CS      | 9         |
| MISO   | 8    | T_IRQ     | 33        |
| CS     | 6    | LED       | 3V3       |
| DC     | 7    | VCC / GND | 3V3 / GND |

- TFT and touch share SCK/MOSI/MISO; each has its own chip select (`CS`, `T_CS`).
- Pins are set by the `custom_tft_*` / `custom_touch_*` build flags.
- `LED` (backlight) ties to 3V3 — full brightness, no dimming.
- Allow ~120 mA on the master's 3V3 supply.

## Touch

`DisplayController::getTouch(&x, &y)` returns the current touch point. The
`x_min/x_max/y_min/y_max` in the touch config are placeholder ranges; run
LovyanGFX `calibrateTouch` once per panel and set the measured values.

## Colour ramp

`src/display/Colormap.h`, packed to RGB565. Tested by `pio test -e native_colormap`.

## Build note

The driver (`LovyanGFX`, vendored under `lib/`) is compiled into the unified
firmware by default. Setting `custom_has_display = 0` strips it for a smaller,
separate binary — only relevant for a build that will never use a screen.
