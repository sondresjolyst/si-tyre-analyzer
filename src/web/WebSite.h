// Copyright (c) 2026 Sondre Sjølyst
// Dark-theme HTML for the config/download page (served in AP/config mode).

#ifndef SRC_WEB_WEBSITE_H_
#define SRC_WEB_WEBSITE_H_

#include <Arduino.h>

#include <vector>

#include "config/DeviceConfig.h"
#include "storage/SessionLogger.h"

namespace tyre {

// Config + pairing + firmware page (with links to Sessions and Live).
String pageRoot(const DeviceConfig &cfg);

// Dedicated sessions list page (grouped by run).
String pageSessions(const std::vector<SessionInfo> &sessions);

// Live 2x2 tyre dashboard (master SoftAP; polls /api/live).
String pageLive();

}  // namespace tyre

#endif  // SRC_WEB_WEBSITE_H_
