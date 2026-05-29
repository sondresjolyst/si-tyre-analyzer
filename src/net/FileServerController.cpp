// Copyright (c) 2026 Sondre Sjølyst

#include "net/FileServerController.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include "config/Settings.h"
#include "controllers/EspNowController.h"
#include "helpers/PRINTHelper.h"
#include "storage/SessionLogger.h"
#include "web/WebSite.h"

extern WebServer server;
extern PRINTHelper printHelper;

namespace tyre {

extern SessionLogger gLogger;
extern DeviceConfig gConfig;
extern EspNowController gEsp;

static const char *kDir = "/sessions";

static void handleRoot() { server.send(200, "text/html", pageRoot(gConfig)); }

static void handleSessionsPage() {
  server.send(200, "text/html", pageSessions(gLogger.listSessions()));
}

static void handleApiSessions() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (const auto &s : gLogger.listSessions()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = s.name;
    o["size"] = s.size;
    o["records"] = s.records;
    o["session_id"] = s.session_id;
    o["wheel"] = wheelName(s.wheel);
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// Reject anything that could escape /sessions (path traversal / absolute).
static bool safeName(const String &n) {
  if (n.length() == 0 || n.length() > 64)
    return false;
  if (n.indexOf('/') >= 0 || n.indexOf('\\') >= 0)
    return false;
  if (n.indexOf("..") >= 0)
    return false;
  return true;
}

static void handleDownload() {
  String name = server.arg("file");
  if (!safeName(name)) {
    server.send(400, "text/plain", "bad file");
    return;
  }
  String path = String(kDir) + "/" + name;
  File f = LittleFS.open(path, "r");
  if (!f) {
    server.send(404, "text/plain", "not found");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=" + name);
  server.streamFile(f, "application/octet-stream");
  f.close();
}

static void handleDelete() {
  String name = server.arg("file");
  if (safeName(name))
    gLogger.deleteSession(name);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

static void handlePair() {
  gEsp.enterPairing();
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

static void handleApiPeers() {
  JsonDocument doc;
  doc["pairing"] = gEsp.pairing();
  JsonArray w = doc["wheels"].to<JsonArray>();
  for (int i = 0; i < kMaxPeers; i++) {
    if (!gConfig.peers[i].in_use)
      continue;
    JsonObject o = w.add<JsonObject>();
    o["wheel"] = wheelName(gConfig.peers[i].wheel);
    o["fw"] = gEsp.peerFw(i);
    o["online"] = gEsp.peerOnline(i);
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleNewCar() {  // master: rotate to a fresh 4-digit Car ID
  gConfig.group_id = 1000 + (esp_random() % 9000);
  // Old peers paired under the previous Car ID are no longer valid.
  for (int i = 0; i < kMaxPeers; i++)
    gConfig.peers[i].in_use = 0;
  saveConfig(gConfig);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

static void handleSetCar() {  // slave: enter the master's Car ID
  if (server.hasArg("group_id")) {
    long v = server.arg("group_id").toInt();
    if (v < 1000)
      v = 1000;
    if (v > 9999)
      v = 9999;
    if (static_cast<uint32_t>(v) != gConfig.group_id) {
      // Different car -> drop the old master pairing; must re-pair.
      gConfig.has_master = 0;
      gConfig.master_ssid[0] = 0;
    }
    gConfig.group_id = static_cast<uint32_t>(v);
    saveConfig(gConfig);
  }
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

static void handleConfig() {
  if (server.hasArg("role"))
    gConfig.role = roleFromName(server.arg("role").c_str());
  if (server.hasArg("wheel"))
    gConfig.wheel = wheelFromName(server.arg("wheel").c_str());
  if (server.hasArg("has_sensor"))
    gConfig.has_sensor = server.arg("has_sensor").toInt() ? 1 : 0;
  if (server.hasArg("channel"))
    gConfig.channel = static_cast<uint8_t>(server.arg("channel").toInt());
  if (server.hasArg("group_id"))
    gConfig.group_id = static_cast<uint32_t>(server.arg("group_id").toInt());
  if (server.hasArg("car_name")) {
    strncpy(gConfig.car_name, server.arg("car_name").c_str(),
            sizeof(gConfig.car_name) - 1);
    gConfig.car_name[sizeof(gConfig.car_name) - 1] = 0;
  }
  saveConfig(gConfig);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

// Receive a wheel unit's session file (multipart upload) and save it under
// /sessions so it joins the master's list, grouped by session_id.
static File gPushFile;
static bool gPushOk = false;

static void handlePushDone() {
  server.send(200, "text/plain", gPushOk ? "OK" : "FAIL");
}

static void handlePushUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    if (gPushFile)
      gPushFile.close();  // close any handle from an aborted run
    gPushOk = false;
    String fn = up.filename;
    int slash = fn.lastIndexOf('/');
    if (slash >= 0)
      fn = fn.substring(slash + 1);
    String path = String(kDir) + "/" + fn;
    gPushFile = LittleFS.open(path, "w");
    printHelper.log("INFO", "receiving session %s", fn.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (gPushFile)
      gPushFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (gPushFile) {
      gPushFile.close();
      gPushOk = true;
    }
    printHelper.log("INFO", "session received: %u bytes", up.totalSize);
  }
}

// Master stores the WHEEL firmware (uploaded by the user) at /fw.bin and
// serves it; "push" tells paired wheels to pull + flash it.
static const char *kFwPath = "/fw.bin";
static File gFwFile;
static bool gFwUploading = false;  // /fw.bin is mid-write; don't serve/push

static void handleFwUploadDone() {
  // Upload finished -> push to all paired wheels.
  if (!gFwUploading && LittleFS.exists(kFwPath))
    gEsp.sendFwPush();
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

static void handleFwUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    if (gFwFile)
      gFwFile.close();
    gFwUploading = true;
    gFwFile = LittleFS.open(kFwPath, "w");
    printHelper.log("INFO", "receiving wheel firmware");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (gFwFile)
      gFwFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (gFwFile)
      gFwFile.close();
    gFwUploading = false;
    printHelper.log("INFO", "wheel firmware stored: %u bytes", up.totalSize);
  }
}

static void handleFwBin() {
  if (gFwUploading) {
    server.send(503, "text/plain", "upload in progress");
    return;
  }
  File f = LittleFS.open(kFwPath, "r");
  if (!f) {
    server.send(404, "text/plain", "no firmware");
    return;
  }
  server.streamFile(f, "application/octet-stream");
  f.close();
}

static void handleUpdateDone() {
  bool ok = !Update.hasError();
  server.send(200, "text/plain", ok ? "OK, rebooting" : "Update FAILED");
  if (ok) {
    delay(200);
    ESP.restart();
  }
}

static void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    printHelper.log("INFO", "OTA start: %s", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      printHelper.log("INFO", "OTA success: %u bytes", up.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

// Captive-portal: redirect any unknown URL (incl. OS probe endpoints like
// /generate_204 and /hotspot-detect.html) to the page so it opens on connect.
static void handleNotFound() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
  server.send(302, "text/plain", "");
}

void registerFileServerRoutes() {
  server.onNotFound(handleNotFound);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/sessions", HTTP_GET, handleSessionsPage);
  server.on("/api/sessions", HTTP_GET, handleApiSessions);
  server.on("/download", HTTP_GET, handleDownload);
  server.on("/api/delete", HTTP_GET, handleDelete);
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/pair", HTTP_POST, handlePair);
  server.on("/car", HTTP_POST, handleSetCar);
  server.on("/car/new", HTTP_POST, handleNewCar);
  server.on("/api/peers", HTTP_GET, handleApiPeers);
  server.on("/fw-upload", HTTP_POST, handleFwUploadDone, handleFwUpload);
  server.on("/fw.bin", HTTP_GET, handleFwBin);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/api/push", HTTP_POST, handlePushDone, handlePushUpload);
}

}  // namespace tyre
