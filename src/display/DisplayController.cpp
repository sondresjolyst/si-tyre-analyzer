// Copyright (c) 2026 Sondre Sjølyst

#if HAS_DISPLAY

#define LGFX_USE_V1
#include <Arduino.h>
#include <LovyanGFX.hpp>

#include "config/DeviceConfig.h"
#include "controllers/LiveDashboard.h"
#include "display/Colormap.h"
#include "display/DisplayController.h"
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
const char *kLabels[4] = {"FL", "FR", "RL", "RR"};

}  // namespace

namespace tyre {

bool DisplayController::begin() {
  if (!gfx.init()) return false;
  gfx.setRotation(1);
  gfx.fillScreen(TFT_BLACK);
  ready_ = true;
  return true;
}

bool DisplayController::getTouch(uint16_t *x, uint16_t *y) {
  if (!ready_) return false;
  return gfx.getTouch(x, y);
}

void DisplayController::render(const LiveDashboard &dash) {
  if (!ready_) return;
  const uint32_t now = millis();
  if (now - lastDrawMs_ < 150) return;
  lastDrawMs_ = now;

  for (int slot = 0; slot < 4; slot++) {
    LiveDashboard::WheelLive w;
    bool valid = dash.snapshot(slot, &w);
    drawCell(slot, kLabels[slot], w.temps, valid);
  }
}

void DisplayController::drawCell(int slot, const char *label,
                                 const int16_t *temps, bool valid) {
  const int cellW = 160, cellH = 120;
  const int x = (slot % 2) * cellW;
  const int y = (slot / 2) * cellH;

  gfx.fillRect(x, y, cellW, cellH, TFT_BLACK);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextSize(2);
  gfx.setCursor(x + 6, y + 4);
  gfx.print(label);

  if (!valid) {
    gfx.setTextColor(0x7BEF, TFT_BLACK);
    gfx.setCursor(x + 60, y + 50);
    gfx.print("--");
    return;
  }

  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < kCols * kRows; i++) {
    float t = temps[i] / kScale;
    if (t < lo) lo = t;
    if (t > hi) hi = t;
  }

  const int mapX = x + 6, mapY = y + 24, mapW = 148, mapH = 64;
  const int pw = mapW / kCols, ph = mapH / kRows;
  for (int r = 0; r < kRows; r++) {
    for (int c = 0; c < kCols; c++) {
      float t = temps[r * kCols + c] / kScale;
      gfx.fillRect(mapX + c * pw, mapY + r * ph, pw, ph,
                   heatRgb565(t, lo, hi));
    }
  }

  const int mid = kRows / 2;
  auto zoneAvg = [&](int c0, int c1) {
    float s = 0;
    for (int c = c0; c < c1; c++) s += temps[mid * kCols + c] / kScale;
    return s / (c1 - c0);
  };
  const int third = kCols / 3;
  int inner = static_cast<int>(zoneAvg(0, third) + 0.5f);
  int middle = static_cast<int>(zoneAvg(third, 2 * third) + 0.5f);
  int outer = static_cast<int>(zoneAvg(2 * third, kCols) + 0.5f);

  gfx.setTextSize(1);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setCursor(x + 6, y + 96);
  gfx.printf("I%d  M%d  O%d C", inner, middle, outer);
}

}  // namespace tyre

#endif  // HAS_DISPLAY
