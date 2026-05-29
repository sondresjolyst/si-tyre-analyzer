// Copyright (c) 2026 Sondre Sjølyst

#include "controllers/LiveDashboard.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>

#include <string.h>

#include "processing/Downsample.h"

extern WebServer server;

namespace tyre {

extern LiveDashboard gDashboard;

// update() runs in the ESP-NOW recv callback (WiFi task); snapshot() runs in
// the loop/WebServer task. Guard the shared per-wheel grid so the reader never
// sees a half-written frame.
static portMUX_TYPE gLiveMux = portMUX_INITIALIZER_UNLOCKED;

void LiveDashboard::update(uint8_t wheel, uint32_t tOffsetMs,
                           const int16_t *temps) {
  if (wheel >= 4) return;
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
    if (!gDashboard.snapshot(i, &w)) continue;
    JsonObject o = wheels[wheelName(i)].to<JsonObject>();
    o["age_ms"] = now - w.last_seen_ms;
    o["t_ms"] = w.t_offset_ms;
    JsonArray t = o["temps"].to<JsonArray>();
    for (int c = 0; c < kGridCells; c++) t.add(unscaleTemp(w.temps[c]));
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleLive() {
  static const char kLivePage[] = R"(<!doctype html><html><head><meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>SI Tyre Analyzer — Live</title>
<style>body{background:#111827;color:#f3f4f6;font-family:-apple-system,'Segoe UI',sans-serif;margin:0;padding:12px}
.wrap{max-width:560px;margin:0 auto}
h1{font-size:16px;margin:0 0 8px}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.tyre{background:#1f2937;border-radius:10px;padding:8px}
.tyre h2{font-size:13px;margin:0 0 6px;text-align:center}
.heat{display:grid;gap:0;border-radius:6px;overflow:hidden}
.cell{aspect-ratio:0.55;display:flex;align-items:center;justify-content:center;
font-size:11px;font-weight:700;color:#fff;text-shadow:0 1px 2px rgba(0,0,0,.6)}
.muted{color:#6b7280;font-size:12px}</style></head><body><div class='wrap'>
<h1>Live tyre temperatures</h1>
<div class='grid'>
<div class='tyre'><h2>FL</h2><div class='heat' id='FL'></div></div>
<div class='tyre'><h2>FR</h2><div class='heat' id='FR'></div></div>
<div class='tyre'><h2>RL</h2><div class='heat' id='RL'></div></div>
<div class='tyre'><h2>RR</h2><div class='heat' id='RR'></div></div>
</div>
<script>
function color(t,min,max){let f=(t-min)/Math.max(1,max-min);f=Math.max(0,Math.min(1,f));
let r=Math.round(255*Math.min(1,f*2)),b=Math.round(255*Math.min(1,(1-f)*2));
return `rgb(${r},${Math.round(80*(1-Math.abs(f-0.5)*2))},${b})`;}
function draw(id,cols,rows,temps){let el=document.getElementById(id);
if(el.children.length!=cols*rows){while(el.firstChild)el.removeChild(el.firstChild);
el.style.gridTemplateColumns='repeat('+cols+',1fr)';
for(let i=0;i<cols*rows;i++){let d=document.createElement('div');d.className='cell';el.appendChild(d);}}
let mn=Math.min(...temps),mx=Math.max(...temps);
for(let i=0;i<temps.length;i++){el.children[i].style.background=color(temps[i],mn,mx);el.children[i].textContent='';}
let mid=Math.floor(rows/2);
for(let c=0;c<cols;c++)el.children[mid*cols+c].textContent=Math.round(temps[mid*cols+c]);}
async function tick(){try{let d=await(await fetch('/api/live')).json();
for(const w of ['FL','FR','RL','RR']){if(d.wheels[w])draw(w,d.cols,d.rows,d.wheels[w].temps);}}catch(e){}}
setInterval(tick,500);tick();
</script></div></body></html>)";
  server.send(200, "text/html", kLivePage);
}

void registerLiveRoutes() {
  server.on("/live", HTTP_GET, handleLive);
  server.on("/api/live", HTTP_GET, handleApiLive);
}

}  // namespace tyre
