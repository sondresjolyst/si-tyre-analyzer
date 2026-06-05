// Copyright (c) 2026 Sondre Sjølyst

#include "config/Settings.h"

#include <Preferences.h>

namespace tyre {

static const char *kNamespace = "tyretemp";

// Per-field storage (not a single struct blob): adding or reordering a field
// in a later firmware just introduces a new key with a default, so a firmware
// update never silently wipes role/wheel/pairing.
bool loadConfig(DeviceConfig *out) {
  out->setDefaults();  // every field gets a sane default first

  Preferences p;
  p.begin(kNamespace, /*readOnly=*/true);
  bool found = p.getBool("init", false);
  if (found) {
    out->role = p.getUChar("role", out->role);
    out->wheel = p.getUChar("wheel", out->wheel);
    out->has_sensor = p.getUChar("has_sensor", out->has_sensor);
    out->has_display = p.getUChar("has_display", out->has_display);
    out->flip_x = p.getUChar("flip_x", out->flip_x);
    out->flip_y = p.getUChar("flip_y", out->flip_y);
    out->channel = p.getUChar("channel", out->channel);
    out->opt_lo = p.getUChar("opt_lo", out->opt_lo);
    out->opt_hi = p.getUChar("opt_hi", out->opt_hi);
    out->group_id = p.getUInt("group", out->group_id);
    String car = p.getString("car", out->car_name);
    strncpy(out->car_name, car.c_str(), sizeof(out->car_name) - 1);
    out->car_name[sizeof(out->car_name) - 1] = 0;
    out->has_master = p.getUChar("has_master", out->has_master);
    p.getBytes("master_mac", out->master_mac, sizeof(out->master_mac));
    String ms = p.getString("master_ssid", out->master_ssid);
    strncpy(out->master_ssid, ms.c_str(), sizeof(out->master_ssid) - 1);
    out->master_ssid[sizeof(out->master_ssid) - 1] = 0;
    if (p.getBytesLength("peers") == sizeof(out->peers))
      p.getBytes("peers", out->peers, sizeof(out->peers));
  }
  p.end();

  if (!found)
    saveConfig(*out);
  return found;
}

void saveConfig(const DeviceConfig &cfg) {
  Preferences p;
  p.begin(kNamespace, /*readOnly=*/false);
  p.putUChar("role", cfg.role);
  p.putUChar("wheel", cfg.wheel);
  p.putUChar("has_sensor", cfg.has_sensor);
  p.putUChar("has_display", cfg.has_display);
  p.putUChar("flip_x", cfg.flip_x);
  p.putUChar("flip_y", cfg.flip_y);
  p.putUChar("channel", cfg.channel);
  p.putUChar("opt_lo", cfg.opt_lo);
  p.putUChar("opt_hi", cfg.opt_hi);
  p.putUInt("group", cfg.group_id);
  p.putString("car", cfg.car_name);
  p.putUChar("has_master", cfg.has_master);
  p.putBytes("master_mac", cfg.master_mac, sizeof(cfg.master_mac));
  p.putString("master_ssid", cfg.master_ssid);
  p.putBytes("peers", cfg.peers, sizeof(cfg.peers));
  p.putBool("init", true);
  p.end();
}

void clearConfig() {
  Preferences p;
  p.begin(kNamespace, /*readOnly=*/false);
  p.clear();
  p.end();
}

}  // namespace tyre
