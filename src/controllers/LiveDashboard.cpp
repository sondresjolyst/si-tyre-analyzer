// Copyright (c) 2026 Sondre Sjølyst

#include "controllers/LiveDashboard.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include <string.h>

#include "processing/Downsample.h"
#include "web/WebSite.h"

extern WebServer server;

namespace tyre {

extern LiveDashboard gDashboard;

// update() runs in the ESP-NOW recv callback (WiFi task); snapshot() runs in
// the loop/WebServer task. Guard the shared per-wheel grid so the reader never
// sees a half-written frame.
static portMUX_TYPE gLiveMux = portMUX_INITIALIZER_UNLOCKED;

void LiveDashboard::update(uint8_t wheel, uint32_t tOffsetMs,
                           const int16_t *temps) {
  if (wheel >= 4)
    return;
  WheelLive &w = wheels_[wheel];
  portENTER_CRITICAL(&gLiveMux);
  memcpy(w.temps, temps, sizeof(int16_t) * kGridCells);
  w.t_offset_ms = tOffsetMs;
  w.last_seen_ms = millis();
  w.valid = true;
  portEXIT_CRITICAL(&gLiveMux);
}

bool LiveDashboard::snapshot(int i, WheelLive *dst) const {
  portENTER_CRITICAL(&gLiveMux);
  *dst = wheels_[i];
  portEXIT_CRITICAL(&gLiveMux);
  return dst->valid;
}

static void handleApiLive() {
  JsonDocument doc;
  doc["cols"] = kGridCols;
  doc["rows"] = kGridRows;
  JsonObject wheels = doc["wheels"].to<JsonObject>();
  const uint32_t now = millis();
  for (int i = 0; i < 4; i++) {
    LiveDashboard::WheelLive w;
    if (!gDashboard.snapshot(i, &w))
      continue;
    JsonObject o = wheels[wheelName(i)].to<JsonObject>();
    o["age_ms"] = now - w.last_seen_ms;
    o["t_ms"] = w.t_offset_ms;
    JsonArray t = o["temps"].to<JsonArray>();
    for (int c = 0; c < kGridCells; c++)
      t.add(unscaleTemp(w.temps[c]));
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleLive() { server.send(200, "text/html", pageLive()); }

void registerLiveRoutes() {
  server.on("/live", HTTP_GET, handleLive);
  server.on("/api/live", HTTP_GET, handleApiLive);
}

}  // namespace tyre
