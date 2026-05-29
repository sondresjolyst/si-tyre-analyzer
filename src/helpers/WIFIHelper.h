// Copyright (c) 2026 Sondre Sjølyst
// SoftAP + captive DNS + Telnet bridge for config/download. Also owns the
// RTC "force AP on next boot" flag (set by the 8 s button hold).

#ifndef SRC_HELPERS_WIFIHELPER_H_
#define SRC_HELPERS_WIFIHELPER_H_

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_attr.h>

#include "helpers/PRINTHelper.h"

extern DNSServer dnsServer;
extern WebServer server;
extern WiFiServer telnetServer;
extern WiFiClient telnetClient;
extern PRINTHelper printHelper;

extern uint32_t gFlagForceAPNextBoot;  // RTC_NOINIT (defined in main.cpp)
constexpr uint32_t FORCE_AP_MAGIC = 0xA9B1C2D3;

// The device's SoftAP SSID ("SITA <wheel> <car-or-mac>").
String apSsid();

// Bring up SoftAP on the given channel + captive DNS + telnet.
void setupAP(uint8_t channel);

// Serial <-> Telnet bridge; call from loop while AP is up.
void handleTelnet();

// Set the RTC flag and restart into AP/config mode.
void rebootIntoAP();

#endif  // SRC_HELPERS_WIFIHELPER_H_
