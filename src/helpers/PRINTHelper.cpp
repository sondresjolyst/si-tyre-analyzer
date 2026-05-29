// Copyright (c) 2026 Sondre Sjølyst

#include <WiFiClient.h>

#include <cstdio>

#include "PRINTHelper.h"

PRINTHelper::PRINTHelper(WiFiClient *client) : _client(client) {}

void PRINTHelper::log(const char *level, const char *format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  Serial.printf("[%s] %s\n", level, buf);

  if (_client && _client->connected()) {
    char netbuf[300];
    snprintf(netbuf, sizeof(netbuf), "[%s] %s\n", level, buf);
    _client->print(netbuf);
  }
}
