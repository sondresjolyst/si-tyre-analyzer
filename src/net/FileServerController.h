// Copyright (c) 2026 Sondre Sjølyst
// Registers the HTTP routes for the config/download/OTA web UI on the global
// WebServer. Used in AP/config mode.

#ifndef SRC_NET_FILESERVERCONTROLLER_H_
#define SRC_NET_FILESERVERCONTROLLER_H_

namespace tyre {

// Attach all routes (/, /api/sessions, /download, /api/delete, /config,
// /update) to the global `server`.
void registerFileServerRoutes();

}  // namespace tyre

#endif  // SRC_NET_FILESERVERCONTROLLER_H_
