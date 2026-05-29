// Copyright (c) 2026 Sondre Sjølyst

#include "helpers/WIFIHelper.h"

#include "config/DeviceConfig.h"

namespace tyre {
extern DeviceConfig gConfig;
}

static const int kDnsPort = 53;
// Master owns 192.168.4.1 (slaves upload there). Slaves use a different subnet
// so their own SoftAP IP doesn't collide with the master's when AP_STA.
static const IPAddress kMasterApIp(192, 168, 4, 1);
static const IPAddress kSlaveApIp(192, 168, 5, 1);

String apSsid() {
  char label[24];
  const char *wheel = tyre::wheelName(tyre::gConfig.wheel);
  if (tyre::gConfig.role == tyre::ROLE_MASTER) {
    if (tyre::gConfig.has_sensor && tyre::gConfig.wheel != tyre::WHEEL_NONE)
      snprintf(label, sizeof(label), "Master %s", wheel);
    else
      snprintf(label, sizeof(label), "Master");
  } else {
    snprintf(label, sizeof(label), "%s", wheel);
  }
  char ssid[33];  // 32-char SSID limit + NUL
  if (tyre::gConfig.car_name[0]) {
    snprintf(ssid, sizeof(ssid), "SITA %s %s", label, tyre::gConfig.car_name);
  } else {
    uint64_t chip = ESP.getEfuseMac();
    snprintf(ssid, sizeof(ssid), "SITA %s %02x%02x%02x%02x%02x%02x", label,
             (uint8_t)(chip), (uint8_t)(chip >> 8), (uint8_t)(chip >> 16),
             (uint8_t)(chip >> 24), (uint8_t)(chip >> 32), (uint8_t)(chip >> 40));
  }
  return String(ssid);
}

void setupAP(uint8_t channel) {
  String ssid = apSsid();
  IPAddress apIp =
      (tyre::gConfig.role == tyre::ROLE_MASTER) ? kMasterApIp : kSlaveApIp;

  // AP_STA: own config AP always reachable, STA used for ESP-NOW + upload.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid.c_str(), /*password=*/nullptr, channel);

  dnsServer.start(kDnsPort, "*", apIp);
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  printHelper.log("INFO", "AP up: SSID '%s' ch %u  http://%s", ssid.c_str(),
                  channel, apIp.toString().c_str());
}

void handleTelnet() {
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      telnetClient = telnetServer.available();
    } else {
      telnetServer.available().stop();
    }
  }
  while (telnetClient && telnetClient.available()) {
    Serial.write(telnetClient.read());
  }
}

void rebootIntoAP() {
  gFlagForceAPNextBoot = FORCE_AP_MAGIC;
  printHelper.log("INFO", "Rebooting into AP/config mode...");
  delay(100);
  ESP.restart();
}
