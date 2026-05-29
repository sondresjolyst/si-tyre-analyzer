// Copyright (c) 2026 Sondre Sjølyst

#ifndef SRC_HELPERS_PRINTHELPER_H_
#define SRC_HELPERS_PRINTHELPER_H_

#ifdef ARDUINO

#include <Arduino.h>
#include <WiFiClient.h>
#include <cstdio>

class PRINTHelper {
 public:
  explicit PRINTHelper(WiFiClient *client);
  void log(const char *level, const char *format, ...);

 private:
  WiFiClient *_client;
};

#else  // Native/test stub — no hardware dependencies

class PRINTHelper {
 public:
  PRINTHelper() {}
  void log(const char *, const char *, ...) {}
};

#endif  // ARDUINO

#endif  // SRC_HELPERS_PRINTHELPER_H_
