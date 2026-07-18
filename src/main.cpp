// Copyright (c) 2026 Sondre Sjølyst
// SI Tyre Analyzer firmware entry point. Single firmware for all devices;
// role/wheel/has_sensor come from NVS. Master coordinates sessions over ESP-NOW
// and serves the live dashboard; wheel units log their own grid to LittleFS.

#include <Arduino.h>
#include <DNSServer.h>
#include <FS.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_attr.h>
#include <esp_mac.h>
#include <esp_system.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "config/DeviceConfig.h"
#include "config/Settings.h"
#include "controllers/EspNowController.h"
#include "controllers/LiveDashboard.h"
#include "controllers/RecordingController.h"
#include "display/DisplayController.h"
#include "helpers/ButtonController.h"
#include "helpers/PRINTHelper.h"
#include "helpers/WIFIHelper.h"
#include "net/FileServerController.h"
#include "sensor/SensorFactory.h"
#include "storage/SessionLogger.h"

#ifndef VERSION
#define VERSION "v0.0.0"
#endif
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 2  // override per board (S3 DevKitC RGB is on 48)
#endif
#define BUTTON_PIN 0
#ifndef EFUSE_FLT_PIN
#define EFUSE_FLT_PIN 10  // TPS26600 FLT: open-drain active-low, 100k to 3V3
#endif

DNSServer dnsServer;
WebServer server(80);
WiFiServer telnetServer(23);
WiFiClient telnetClient;
PRINTHelper printHelper(&telnetClient);
RTC_NOINIT_ATTR uint32_t gFlagForceAPNextBoot;

namespace tyre {
SessionLogger gLogger;
DeviceConfig gConfig;
LiveDashboard gDashboard;
EspNowController gEsp;
#if HAS_DISPLAY
DisplayController gDisplay;
#endif
}  // namespace tyre

using tyre::gConfig;
using tyre::gDashboard;
using tyre::gEsp;
using tyre::gLogger;

enum AppState { STATE_IDLE, STATE_RECORDING, STATE_PAIRING, STATE_CONFIG_AP };
static AppState appState = STATE_IDLE;

static tyre::ITempSensor &sensor = tyre::makeSensor();
static tyre::RecordingController recorder(&sensor, &gLogger);
static tyre::ButtonController button(BUTTON_PIN);
#if BUTTON2_PIN >= 0
static tyre::ButtonController button2(BUTTON2_PIN);
#endif

static bool apActive = false;
static uint32_t gSessionId = 0;
static uint32_t lastLedMs = 0;
static uint32_t lastLiveMs = 0;
static bool ledOn = false;

// Set by ESP-NOW callbacks (kept short); acted on in loop().
static volatile bool gStartPending = false;
static volatile uint8_t gStartOptLo = 0;
static volatile uint8_t gStartOptHi = 0;
static volatile bool gStopPending = false;
// A finished slave session waiting to be uploaded to the master.
static bool gUploadPending = false;
static uint32_t gUploadSessionId = 0;
// Master asked this wheel to pull + flash new firmware.
static volatile bool gFwPushPending = false;
// Master asked this wheel to upload every stored session.
static volatile bool gSyncPending = false;
// When a sensorless master started the current session (for auto-stop).
static uint32_t gMasterStartMs = 0;

// ── Recording control ────────────────────────────────────────────────────────
static uint32_t nextSessionNumber() {
  Preferences p;
  p.begin("tyrelog", /*readOnly=*/false);
  uint32_t n = p.getUInt("session", 0) + 1;
  p.putUInt("session", n);
  p.end();
  return n;
}

static void startLocalSession(uint32_t sessionId, uint8_t optLo,
                              uint8_t optHi) {
  if (!gConfig.has_sensor)
    return;
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  gSessionId = sessionId;
  if (recorder.start(sessionId, /*epochMs=*/0, gConfig.wheel, mac, VERSION,
                     gConfig.group_id, gConfig.car_name, optLo, optHi)) {
    appState = STATE_RECORDING;
  }
}

static void stopLocalSession() {
  const bool wasRecording = recorder.isRecording();
  recorder.stop();
  if (appState == STATE_RECORDING)
    appState = STATE_IDLE;
  // Wheel units hand their finished file to the master.
  if (wasRecording && gConfig.has_sensor && gConfig.has_master) {
    gUploadSessionId = gSessionId;
    gUploadPending = true;
  }
}

// Master tap (or serial "rec"): start/stop a coordinated session.
static void toggleRecording() {
  if (appState == STATE_CONFIG_AP)
    return;
  if (appState == STATE_PAIRING) {  // a tap cancels pairing
    gEsp.stopPairing();
    appState = STATE_IDLE;
    return;
  }
  // Recording is master-driven; a wheel unit records only on the master's
  // START/STOP, never from its own button.
  if (gConfig.role != tyre::ROLE_MASTER)
    return;

  if (recorder.isRecording() || appState == STATE_RECORDING) {
    if (gConfig.role == tyre::ROLE_MASTER)
      gEsp.sendStop(gSessionId);
    stopLocalSession();
    return;
  }

  uint32_t sessionId = nextSessionNumber();
  if (gConfig.role == tyre::ROLE_MASTER) {
    gEsp.sendStart(sessionId, static_cast<uint16_t>(SAMPLE_RATE_HZ),
                   gConfig.opt_lo, gConfig.opt_hi);
    gSessionId = sessionId;
    gMasterStartMs = millis();
    appState = STATE_RECORDING;  // master tracks session even without a sensor
  }
  startLocalSession(sessionId, gConfig.opt_lo, gConfig.opt_hi);
}

// Non-static so the web layer (FileServerController) can drive recording.
void uiToggleRecording() { toggleRecording(); }
bool uiRecording() {
  return recorder.isRecording() || appState == STATE_RECORDING;
}

// Fill `out` with a native-resolution frame (tyre::MLX_PIXELS values) for the
// alignment view. False if this unit has no sensor or a frame can't be read.
bool uiReadAlignFrame(int16_t *out) {
  return gConfig.has_sensor && recorder.readAlignFrame(out);
}

static void enterPairing() {
  gEsp.enterPairing();
  appState = STATE_PAIRING;
}

// 8 s hold -> config AP. Stop + flush any active recording first so the
// session file isn't left truncated.
static void enterConfigAP() {
  if (appState == STATE_RECORDING) {
    if (gConfig.role == tyre::ROLE_MASTER)
      gEsp.sendStop(gSessionId);
    stopLocalSession();
  }
  rebootIntoAP();
}

// ── ESP-NOW callbacks (run in WiFi task — keep short, defer to loop)
// ──────────
static void onEspStart(uint32_t sessionId, uint16_t /*rateHz*/, uint8_t optLo,
                       uint8_t optHi) {
  gSessionId = sessionId;
  gStartOptLo = optLo;
  gStartOptHi = optHi;
  gStartPending = true;
}
static void onEspStop(uint32_t /*sessionId*/) { gStopPending = true; }
static void onEspLive(uint8_t wheel, uint32_t tOffsetMs, const int16_t *temps) {
  gDashboard.update(wheel, tOffsetMs, temps);
}
static void onEspFwPush() { gFwPushPending = true; }
static void onEspSync() { gSyncPending = true; }

// Associate STA to the master's (open) SoftAP. Returns whether connected.
// Connect by explicit channel + BSSID (both stored at pairing) rather than by
// SSID alone: in AP_STA with ESP-NOW active a scan-based WiFi.begin(ssid) fails
// with WL_NO_SSID_AVAIL, and the master's SoftAP shares our ESP-NOW channel.
static bool joinMasterAP(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED)
    return true;
  WiFi.begin(gConfig.master_ssid, /*password=*/nullptr, gConfig.channel,
             gConfig.master_mac);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs)
    delay(100);
  return WiFi.status() == WL_CONNECTED;
}

// Wheel unit: pull /fw.bin from the master AP and self-flash, then reboot.
static bool downloadFirmwareFromMaster() {
  if (!gConfig.has_master || gConfig.master_ssid[0] == 0)
    return false;
  printHelper.log("INFO", "fw push: joining master to download");
  if (!joinMasterAP(10000)) {
    printHelper.log("WARN", "fw: join failed");
    return false;
  }
  HTTPClient http;
  http.begin("http://192.168.4.1/fw.bin");
  int code = http.GET();
  if (code != 200) {
    printHelper.log("WARN", "fw: GET %d", code);
    http.end();
    return false;
  }
  int len = http.getSize();
  if (!Update.begin(len > 0 ? len : UPDATE_SIZE_UNKNOWN)) {
    printHelper.log("WARN", "fw: Update.begin failed");
    http.end();
    return false;
  }
  size_t written = Update.writeStream(*http.getStreamPtr());
  http.end();
  if (Update.end(true)) {
    printHelper.log("INFO", "fw: flashed %u bytes, rebooting",
                    (unsigned)written);
    delay(200);
    ESP.restart();
    return true;
  }
  printHelper.log("WARN", "fw: Update.end failed");
  return false;
}

// Wheel unit -> master: join the master SoftAP and POST one .bin. Retried, so
// a wheel that briefly can't reach the master AP still gets its file across.
// The master stores by filename (idempotent), so re-sending an old file is safe.
static bool postFileToMaster(const String &path, const String &fname) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    printHelper.log("WARN", "upload skip: open failed %s", path.c_str());
    return false;
  }
  const size_t fsize = f.size();
  printHelper.log("INFO", "upload start: %s (%u B) -> '%s'", fname.c_str(),
                  (unsigned)fsize, gConfig.master_ssid);

  const IPAddress masterIp(192, 168, 4, 1);
  const String boundary = "----sitatyre";
  String hdr =
      "--" + boundary +
      "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + fname +
      "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  size_t clen = hdr.length() + fsize + tail.length();

  for (int attempt = 0; attempt < 4; attempt++) {
    WiFiClient c;
    if (!joinMasterAP(8000)) {
      printHelper.log("WARN", "upload attempt %d: join '%s' failed (status=%d)",
                      attempt + 1, gConfig.master_ssid, (int)WiFi.status());
      delay(1500UL + attempt * 1500UL);
      continue;
    }
    if (!c.connect(masterIp, 80)) {
      printHelper.log("WARN", "upload attempt %d: joined but connect :80 failed",
                      attempt + 1);
      delay(1500UL + attempt * 1500UL);
      continue;
    }
    c.print(String("POST /api/push HTTP/1.1\r\nHost: 192.168.4.1\r\n") +
            "Content-Type: multipart/form-data; boundary=" + boundary +
            "\r\nContent-Length: " + clen + "\r\nConnection: close\r\n\r\n");
    c.print(hdr);
    f.seek(0);
    uint8_t buf[512];
    while (f.available()) {
      int n = f.read(buf, sizeof(buf));
      c.write(buf, n);
    }
    c.print(tail);
    uint32_t r0 = millis();
    while (c.connected() && millis() - r0 < 3000) {
      while (c.available())
        c.read();
      delay(10);
    }
    c.stop();
    f.close();
    printHelper.log("INFO", "upload done: %s (%u B)", fname.c_str(),
                    (unsigned)fsize);
    return true;
  }
  f.close();
  printHelper.log("WARN", "upload failed after retries: %s", fname.c_str());
  return false;
}

// Session-file basename -> ("/sessions/<name>", "<name>"). Handles both bare and
// path-prefixed dir entries.
static void sessionPath(const String &name, String *path, String *fname) {
  *fname = name.substring(name.lastIndexOf('/') + 1);
  *path = String("/sessions/") + *fname;
}

// Upload the session just finished, matched by its "_<wheel>_<id>.bin" suffix.
// Staggered by wheel so four wheels uploading after a shared STOP don't collide.
static bool uploadSessionToMaster(uint32_t sessionId) {
  if (!gConfig.has_master || gConfig.master_ssid[0] == 0) {
    printHelper.log("WARN", "upload skip: no master (has_master=%u ssid='%s')",
                    gConfig.has_master, gConfig.master_ssid);
    return false;
  }
  char suffix[24];
  snprintf(suffix, sizeof(suffix), "_%s_%u.bin", tyre::wheelName(gConfig.wheel),
           static_cast<unsigned>(sessionId));
  String path, fname;
  File dir = LittleFS.open("/sessions");
  for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
    String n = String(e.name());
    if (!e.isDirectory() && n.endsWith(suffix)) {
      sessionPath(n, &path, &fname);
      break;
    }
  }
  if (path.isEmpty()) {
    printHelper.log("WARN", "upload skip: no file matching *%s", suffix);
    return false;
  }
  if (gConfig.wheel < 4)
    delay(gConfig.wheel * 1500UL);
  return postFileToMaster(path, fname);
}

// Master-triggered (MSG_SYNC): push every stored session to the master, oldest
// first. Names are collected up front so no dir handle is held across the WiFi
// work. Master overwrites by filename, so already-present files are harmless.
static void uploadAllSessions() {
  if (!gConfig.has_master || gConfig.master_ssid[0] == 0) {
    printHelper.log("WARN", "sync skip: no master");
    return;
  }
  std::vector<String> names;
  for (const auto &s : gLogger.listSessions())
    names.push_back(s.name);
  std::sort(names.begin(), names.end());
  printHelper.log("INFO", "sync: %u session(s)", (unsigned)names.size());
  if (gConfig.wheel < 4)
    delay(gConfig.wheel * 1500UL);  // stagger wheels once
  for (const auto &n : names) {
    String path, fname;
    sessionPath(n, &path, &fname);
    postFileToMaster(path, fname);
  }
  printHelper.log("INFO", "sync: done");
}

// ── LED feedback ─────────────────────────────────────────────────────────────
static void setLed(bool on) {
  ledOn = on;
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}
static void blink(uint32_t period) {
  uint32_t now = millis();
  if (now - lastLedMs >= period) {
    lastLedMs = now;
    setLed(!ledOn);
  }
}
static void updateLed() {
  tyre::ButtonStage st = button.stage();
#if BUTTON2_PIN >= 0
  if (button2.stage() > st)
    st = button2.stage();
#endif
  switch (st) {
  case tyre::STAGE_PAIR:
    blink(120);
    return;
  case tyre::STAGE_AP:
    setLed(true);
    return;
  default:
    break;
  }
  switch (appState) {
  case STATE_RECORDING:
    setLed(true);
    break;
  case STATE_PAIRING:
    blink(120);
    break;
  case STATE_CONFIG_AP:
    blink(200);
    break;
  default:
    blink(ledOn ? 60 : 2000);
    break;
  }
}

// ── Serial console (drive the device without the physical button) ────────────
static void dumpFile(const String &name) {
  String path = "/sessions/" + name;
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("ERR no file %s\n", name.c_str());
    return;
  }
  Serial.printf("DUMP-BEGIN %s %u\n", name.c_str(), (unsigned)f.size());
  static const char *hex = "0123456789abcdef";
  char line[129];
  int col = 0;
  while (f.available()) {
    uint8_t b = f.read();
    line[col++] = hex[b >> 4];
    line[col++] = hex[b & 0xF];
    if (col >= 128) {
      line[col] = 0;
      Serial.println(line);
      col = 0;
    }
  }
  if (col) {
    line[col] = 0;
    Serial.println(line);
  }
  Serial.println("DUMP-END");
  f.close();
}

static void handleCommand(String cmd) {
  cmd.trim();
  if (cmd == "info") {
    Serial.printf("role=%s wheel=%s has_sensor=%u ch=%u group=0x%06X car='%s' "
                  "master_ssid='%s' optimal=%u-%u state=%d fs=%u/%u recording=%d\n",
                  gConfig.role == tyre::ROLE_MASTER ? "master" : "slave",
                  tyre::wheelName(gConfig.wheel), gConfig.has_sensor,
                  gConfig.channel, gConfig.group_id, gConfig.car_name,
                  gConfig.master_ssid, gConfig.opt_lo, gConfig.opt_hi, appState,
                  (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes(),
                  recorder.isRecording());
  } else if (cmd == "rec") {
    toggleRecording();
    Serial.println("OK rec");
  } else if (cmd == "stop") {
    if (gConfig.role == tyre::ROLE_MASTER)
      gEsp.sendStop(gSessionId);
    stopLocalSession();
    Serial.println("OK stop");
  } else if (cmd == "ls") {
    for (const auto &s : gLogger.listSessions())
      Serial.printf("%s  %u B  %u recs  wheel=%s\n", s.name.c_str(),
                    (unsigned)s.size, (unsigned)s.records,
                    tyre::wheelName(s.wheel));
  } else if (cmd.startsWith("dump ")) {
    dumpFile(cmd.substring(5));
  } else if (cmd.startsWith("rm ")) {
    gLogger.deleteSession(cmd.substring(3));
    Serial.println("OK rm");
  } else if (cmd == "pair") {
    enterPairing();
    Serial.println("OK pair");
  } else if (cmd == "ap") {
    rebootIntoAP();
  } else if (cmd.startsWith("wheel ")) {
    gConfig.wheel = tyre::wheelFromName(cmd.substring(6).c_str());
    tyre::saveConfig(gConfig);
    Serial.println("OK wheel");
  } else if (cmd.startsWith("role ")) {
    gConfig.role = tyre::roleFromName(cmd.substring(5).c_str());
    tyre::saveConfig(gConfig);
    Serial.println("OK role (reboot to apply)");
  } else if (cmd.startsWith("sensor ")) {
    gConfig.has_sensor = cmd.substring(7).toInt() ? 1 : 0;
    tyre::saveConfig(gConfig);
    Serial.println("OK sensor (reboot to apply)");
  } else if (cmd.startsWith("display ")) {
    gConfig.has_display = cmd.substring(8).toInt() ? 1 : 0;
    tyre::saveConfig(gConfig);
    Serial.println("OK display (reboot to apply)");
  } else if (cmd.startsWith("flip ")) {
    const String a = cmd.substring(5);
    const int sp = a.indexOf(' ');
    gConfig.flip_x = a.substring(0, sp).toInt() ? 1 : 0;
    gConfig.flip_y = (sp >= 0 ? a.substring(sp + 1).toInt() : 0) ? 1 : 0;
    tyre::saveConfig(gConfig);
    recorder.setFlip(gConfig.flip_x, gConfig.flip_y);
    Serial.printf("OK flip x=%u y=%u\n", gConfig.flip_x, gConfig.flip_y);
  } else if (cmd.startsWith("car ")) {
    gConfig.group_id =
        static_cast<uint32_t>(strtoul(cmd.substring(4).c_str(), nullptr, 0));
    tyre::saveConfig(gConfig);
    Serial.printf("OK car id=0x%06X\n", gConfig.group_id);
  } else if (cmd.startsWith("optimal ")) {
    const String a = cmd.substring(8);
    const int sp = a.indexOf(' ');
    gConfig.opt_lo = a.substring(0, sp).toInt();
    gConfig.opt_hi = (sp >= 0 ? a.substring(sp + 1).toInt() : 0);
    tyre::saveConfig(gConfig);
    Serial.printf("OK optimal %u-%u C\n", gConfig.opt_lo, gConfig.opt_hi);
  } else if (cmd == "reboot") {
    Serial.println("OK reboot");
    delay(100);
    ESP.restart();
  } else if (cmd == "pushfw") {
    gEsp.sendFwPush();
    Serial.println("OK pushfw");
  } else if (cmd == "peers") {
    for (int i = 0; i < tyre::kMaxPeers; i++)
      if (gConfig.peers[i].in_use)
        Serial.printf("peer %d wheel=%s fw=%s %s\n", i,
                      tyre::wheelName(gConfig.peers[i].wheel), gEsp.peerFw(i),
                      gEsp.peerOnline(i) ? "online" : "offline");
  } else if (cmd.length()) {
    Serial.println("cmds: info rec stop ls dump<f> rm<f> pair ap reboot "
                   "wheel<x> role<x> sensor<0|1> car<id>");
  }
}

static void pollSerial() {
  static String buf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buf.length())
        handleCommand(buf);
      buf = "";
    } else {
      buf += c;
    }
  }
}

// ── Setup ────────────────────────────────────────────────────────────────────
static void startWebServer(bool withLive) {
  tyre::registerFileServerRoutes();
  if (withLive)
    tyre::registerLiveRoutes();
  server.begin();
}

void setup() {
  Serial.begin(9600);
  pinMode(STATUS_LED_PIN, OUTPUT);
  setLed(false);
  pinMode(EFUSE_FLT_PIN, INPUT);  // ext 100k pull-up to 3V3; LOW = tripped

  tyre::loadConfig(&gConfig);
  gLogger.begin();

  button.begin();
  button.onTap(toggleRecording);
  button.onPair(enterPairing);
  button.onAp(enterConfigAP);
#if BUTTON2_PIN >= 0
  button2.begin();
  button2.onTap(toggleRecording);
  button2.onPair(enterPairing);
  button2.onAp(enterConfigAP);
#endif

  const bool forceAP = (gFlagForceAPNextBoot == FORCE_AP_MAGIC);
  gFlagForceAPNextBoot = 0;

  printHelper.log("INFO", "SI Tyre Analyzer %s role=%s wheel=%s sensor=%d",
                  VERSION,
                  (gConfig.role == tyre::ROLE_MASTER) ? "master" : "slave",
                  tyre::wheelName(gConfig.wheel), gConfig.has_sensor);
  printHelper.log("INFO", "reset reason=%d heap=%u", (int)esp_reset_reason(),
                  (unsigned)ESP.getFreeHeap());

  if (forceAP) {
    setupAP(gConfig.channel);
    startWebServer(/*withLive=*/gConfig.role == tyre::ROLE_MASTER);
    apActive = true;
    appState = STATE_CONFIG_AP;
    return;
  }

  // Every device keeps its own SoftAP up so it is always configurable; the
  // master also serves the live dashboard.
  setupAP(gConfig.channel);
  startWebServer(/*withLive=*/gConfig.role == tyre::ROLE_MASTER);
  apActive = true;

  gEsp.begin(&gConfig);
  gEsp.onStart(onEspStart);
  gEsp.onStop(onEspStop);
  gEsp.onLive(onEspLive);
  gEsp.onFwPush(onEspFwPush);
  gEsp.onSync(onEspSync);

  recorder.setFlip(gConfig.flip_x, gConfig.flip_y);
  if (gConfig.has_sensor && !recorder.beginSensor()) {
    printHelper.log("ERROR", "sensor begin failed");
  }
#if HAS_DISPLAY
  if (gConfig.role == tyre::ROLE_MASTER && gConfig.has_display &&
      !tyre::gDisplay.begin())
    printHelper.log("ERROR", "display begin failed");
#endif
  appState = STATE_IDLE;

  // A configured-but-unpaired wheel (Car ID set) auto-listens so the car
  // self-pairs at power-on. Without a Car ID, pairing must be deliberate.
  if (gConfig.role == tyre::ROLE_SLAVE && !gConfig.has_master &&
      gConfig.group_id != 0) {
    gEsp.enterPairing();
  }
}

// ── 12 V eFuse fault (TPS26600 FLT, active-low) ──────────────────────────────
static bool gEfuseFaulted = false;
static void pollEfuseFault() {
  // FLT is open-drain, pulled to 3V3; LOW = eFuse tripped (over-current,
  // over-voltage, or thermal). The -00 variant auto-retries, so it self-clears.
  const bool faulted = (digitalRead(EFUSE_FLT_PIN) == LOW);
  if (faulted == gEfuseFaulted)
    return;
  gEfuseFaulted = faulted;
  printHelper.log(faulted ? "WARN" : "INFO",
                  faulted ? "12V input eFuse fault (FLT low)"
                          : "12V input eFuse recovered");
}

// ── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  button.update();
#if BUTTON2_PIN >= 0
  button2.update();
#endif
  gEsp.update();
  pollSerial();
  updateLed();
  pollEfuseFault();
#if HAS_DISPLAY
  if (gConfig.role == tyre::ROLE_MASTER && gConfig.has_display)
    tyre::gDisplay.render(gDashboard);
#endif

  // Reflect web-/auto-triggered pairing in the app state (for the LED).
  if (gEsp.pairing() && appState == STATE_IDLE)
    appState = STATE_PAIRING;
  if (appState == STATE_PAIRING && !gEsp.pairing())
    appState = STATE_IDLE;

  // Deferred ESP-NOW actions (set in the recv callback).
  if (gStartPending) {
    gStartPending = false;
    startLocalSession(gSessionId, gStartOptLo, gStartOptHi);
  }
  if (gStopPending) {
    gStopPending = false;
    stopLocalSession();
  }
  if (gUploadPending) {
    gUploadPending = false;
    uploadSessionToMaster(gUploadSessionId);
  }
  if (gFwPushPending) {
    gFwPushPending = false;
    downloadFirmwareFromMaster();
  }
  if (gSyncPending) {
    gSyncPending = false;
    uploadAllSessions();
  }

  if (apActive || appState == STATE_CONFIG_AP) {
    dnsServer.processNextRequest();
    server.handleClient();
    handleTelnet();
  }

  if (appState == STATE_RECORDING) {
    recorder.tick();
    // Stream a throttled live grid (~2 Hz). A wheel sends it to the master; a
    // sensor-equipped master (no master of its own) feeds the dashboard direct.
    if (gConfig.has_sensor && millis() - lastLiveMs >= 500) {
      lastLiveMs = millis();
      if (gConfig.has_master)
        gEsp.sendLiveGrid(gSessionId, recorder.lastSampleOffsetMs(),
                          recorder.lastScaledGrid());
      else
        gDashboard.update(gConfig.wheel, recorder.lastSampleOffsetMs(),
                          recorder.lastScaledGrid());
    }
    if (gConfig.has_sensor && !recorder.isRecording())
      appState = STATE_IDLE;

    // A sensorless master has no local recorder to auto-stop it, so end the
    // session (and tell the wheels) at SESSION_MAX_SEC.
    if (gConfig.role == tyre::ROLE_MASTER && !gConfig.has_sensor &&
        millis() - gMasterStartMs >=
            static_cast<uint32_t>(SESSION_MAX_SEC) * 1000UL) {
      gEsp.sendStop(gSessionId);
      stopLocalSession();
    }
  }
}
