// Copyright (c) 2026 Sondre Sjølyst

#if HAS_DISPLAY

#include "display/DisplayController.h"

#include <cstdio>
#include <cstring>

#define LGFX_USE_V1
#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "config/DeviceConfig.h"
#include "controllers/LiveDashboard.h"
#include "display/Colormap.h"
#include "processing/Downsample.h"
#include "storage/LogFormat.h"

namespace {

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Touch_XPT2046 touch_;

 public:
  LGFX() {
    auto b = bus_.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 40000000;
    b.freq_read = 16000000;
    b.pin_sclk = TFT_SCK;
    b.pin_mosi = TFT_MOSI;
    b.pin_miso = TFT_MISO;
    b.pin_dc = TFT_DC;
    b.dma_channel = SPI_DMA_CH_AUTO;
    bus_.config(b);
    panel_.setBus(&bus_);

    auto p = panel_.config();
    p.pin_cs = TFT_CS;
    p.pin_rst = TFT_RST;
    p.pin_busy = -1;
    p.panel_width = 240;
    p.panel_height = 320;
    p.readable = true;
    p.bus_shared = true;
    panel_.config(p);

    auto t = touch_.config();
    t.spi_host = SPI2_HOST;
    t.pin_sclk = TFT_SCK;
    t.pin_mosi = TFT_MOSI;
    t.pin_miso = TFT_MISO;
    t.pin_cs = TOUCH_CS;
    t.pin_int = TOUCH_IRQ;
    t.bus_shared = true;
    t.freq = 1000000;
    t.x_min = 300;
    t.x_max = 3900;
    t.y_min = 300;
    t.y_max = 3900;
    touch_.config(t);
    panel_.setTouch(&touch_);

    setPanel(&panel_);
  }
};

LGFX gfx;

constexpr int kCols = tyre::kGridCols;
constexpr int kRows = tyre::kGridRows;
constexpr float kScale = static_cast<float>(tyre::kTempScale);

constexpr int kCellW = 160, kCellH = 120;

const uint16_t kBg = tyre::toRgb565(tyre::Rgb{0x11, 0x18, 0x27});
const uint16_t kTile = tyre::toRgb565(tyre::Rgb{0x1f, 0x29, 0x37});
const uint16_t kMuted = tyre::toRgb565(tyre::Rgb{0x9c, 0xa3, 0xaf});
const uint16_t kGrn = tyre::toRgb565(tyre::Rgb{0x22, 0xc5, 0x5e});
const uint16_t kTxt = tyre::toRgb565(tyre::Rgb{0xf3, 0xf4, 0xf6});

}  // namespace

namespace tyre {

extern DeviceConfig gConfig;

bool DisplayController::begin() {
  if (!gfx.init())
    return false;
  gfx.setRotation(1);
  gfx.fillScreen(kBg);
  ready_ = true;
  return true;
}

bool DisplayController::getTouch(uint16_t *x, uint16_t *y) {
  if (!ready_)
    return false;
  return gfx.getTouch(x, y);
}

void DisplayController::render(const LiveDashboard &dash) {
  if (!ready_)
    return;
  const uint32_t now = millis();
  if (now - lastDrawMs_ < 150)
    return;
  lastDrawMs_ = now;

  for (int slot = 0; slot < 4; slot++) {
    LiveDashboard::WheelLive w;
    bool valid = dash.snapshot(slot, &w);
    drawCell(slot, w.temps, valid);
  }
}

void DisplayController::drawCell(int slot, const int16_t *temps, bool valid) {
  const int x = (slot % 2) * kCellW;
  const int y = (slot / 2) * kCellH;
  const int m = 2;
  const int tx = x + m, ty = y + m, tw = kCellW - 2 * m, th = kCellH - 2 * m;

  gfx.fillRect(x, y, kCellW, kCellH, kBg);
  gfx.fillRoundRect(tx, ty, tw, th, 6, kTile);

  gfx.fillCircle(tx + tw - 9, ty + 8, 4, valid ? kGrn : kMuted);

  if (!valid) {
    gfx.setTextColor(kMuted, kTile);
    gfx.setTextSize(2);
    gfx.setCursor(tx + tw / 2 - 12, ty + th / 2 - 8);
    gfx.print("--");
    return;
  }

  const int mapX = tx + 4, mapY = ty + 14, mapW = tw - 8, mapH = th - 20;
  const float optLo = static_cast<float>(gConfig.opt_lo);
  const float optHi = static_cast<float>(gConfig.opt_hi);
  for (int r = 0; r < kRows; r++) {
    const int y0 = mapY + r * mapH / kRows, y1 = mapY + (r + 1) * mapH / kRows;
    for (int c = 0; c < kCols; c++) {
      const int x0 = mapX + c * mapW / kCols,
                x1 = mapX + (c + 1) * mapW / kCols;
      float t = temps[r * kCols + c] / kScale;
      gfx.fillRect(x0, y0, x1 - x0, y1 - y0, heatRgb565(t, optLo, optHi));
    }
  }

  const int third = kCols / 3;
  auto zoneAvg = [&](int c0, int c1) {
    float s = 0.0f;
    for (int r = 0; r < kRows; r++)
      for (int c = c0; c < c1; c++)
        s += temps[r * kCols + c] / kScale;
    return s / ((c1 - c0) * kRows);
  };
  const int zone[3] = {static_cast<int>(zoneAvg(0, third) + 0.5f),
                       static_cast<int>(zoneAvg(third, 2 * third) + 0.5f),
                       static_cast<int>(zoneAvg(2 * third, kCols) + 0.5f)};
  const int cy = mapY + mapH / 2;
  const float zs = 2.3f;
  const int gw = static_cast<int>(6 * zs + 0.5f);
  const int gh = static_cast<int>(8 * zs + 0.5f);
  gfx.setTextSize(zs);
  for (int z = 0; z < 3; z++) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", zone[z]);
    const int chipW = gw * static_cast<int>(strlen(buf)) + 6, chipH = gh + 4;
    const int cx = mapX + mapW * (2 * z + 1) / 6;
    const int bx = cx - chipW / 2, by = cy - chipH / 2;
    gfx.fillRoundRect(bx, by, chipW, chipH, 3, kBg);
    gfx.setTextColor(kTxt, kBg);
    gfx.setCursor(bx + 3, by + 2);
    gfx.print(buf);
  }
}

}  // namespace tyre

#endif  // HAS_DISPLAY
