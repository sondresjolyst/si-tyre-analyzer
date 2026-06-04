// Copyright (c) 2026 Sondre Sjølyst

#include "controllers/EspNowController.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

#include "config/Settings.h"
#include "helpers/PRINTHelper.h"
#include "helpers/WIFIHelper.h"

#ifndef VERSION
#define VERSION "v0.0.0"
#endif

extern PRINTHelper printHelper;

namespace tyre {

EspNowController *EspNowController::self_ = nullptr;

static const uint8_t kBroadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint32_t kAnnounceIntervalMs = 500;
static const uint32_t kPairTimeoutMs = 60000;
// Only pair with a device held close (strong signal), so a neighbouring car
// pairing at the same time is too weak to match.
static const int kPairRssiMin = -55;

static uint32_t macCarId() {
  uint8_t mac[6] = {0};
  esp_wifi_get_mac(WIFI_IF_AP, mac);
  uint32_t h = (static_cast<uint32_t>(mac[3]) << 16) |
               (static_cast<uint32_t>(mac[4]) << 8) | mac[5];
  return 1000 + (h % 9000);
}

bool EspNowController::begin(DeviceConfig *cfg) {
  cfg_ = cfg;
  self_ = this;

  if (cfg_->role == ROLE_MASTER) {
    ifidx_ = WIFI_IF_AP;  // SoftAP brought up by caller on cfg_->channel
    if (cfg_->group_id == 0) {
      cfg_->group_id = macCarId();
      saveConfig(*cfg_);
    }
  } else {
    ifidx_ = WIFI_IF_STA;  // SoftAP (AP_STA) already up on cfg_->channel
  }

  if (esp_now_init() != ESP_OK) {
    printHelper.log("ERROR", "esp_now_init failed");
    return false;
  }
  esp_now_register_recv_cb(recvThunk);

  if (cfg_->role == ROLE_MASTER)
    addPeer(kBroadcast);
  addStoredPeers();

  // Restore last-known peer firmware versions (shown in the web list).
  {
    Preferences p;
    p.begin("tyrenow", /*readOnly=*/true);
    if (p.getBytesLength("peerfw") == sizeof(peerFw_))
      p.getBytes("peerfw", peerFw_, sizeof(peerFw_));
    p.end();
  }

  printHelper.log("INFO", "esp-now up: group=0x%06X ch=%u role=%s",
                  cfg_->group_id, cfg_->channel,
                  cfg_->role == ROLE_MASTER ? "master" : "slave");
  return true;
}

void EspNowController::addPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac))
    return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;  // use current channel
  peer.ifidx = static_cast<wifi_interface_t>(ifidx_);
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

void EspNowController::addStoredPeers() {
  if (cfg_->role == ROLE_MASTER) {
    for (int i = 0; i < kMaxPeers; i++) {
      if (cfg_->peers[i].in_use)
        addPeer(cfg_->peers[i].mac);
    }
  } else if (cfg_->has_master) {
    addPeer(cfg_->master_mac);
  }
}

void EspNowController::fillHeader(MsgHeader *h, uint8_t type) const {
  h->type = type;
  h->wheel = cfg_->wheel;
  h->role = cfg_->role;
  h->_pad = 0;
  h->group_id = cfg_->group_id;
}

bool EspNowController::accept(const MsgHeader *h) const {
  return cfg_->group_id != 0 && h->group_id == cfg_->group_id;
}

void EspNowController::enterPairing() {
  if (cfg_->role == ROLE_MASTER && cfg_->group_id == 0) {
    cfg_->group_id = macCarId();
    saveConfig(*cfg_);
  }
  pairing_ = true;
  pairStartMs_ = millis();
  lastAnnounceMs_ = 0;
  printHelper.log("INFO", "pairing started");
}

void EspNowController::update() {
  if (!cfg_)
    return;  // begin() not called (e.g. AP/config-only boot)
  const uint32_t now = millis();

  // Slave heartbeat: keep the master's version list fresh + show liveness,
  // independent of pairing/reboot timing.
  if (cfg_->role == ROLE_SLAVE && cfg_->has_master &&
      now - lastHeartbeatMs_ >= 5000) {
    lastHeartbeatMs_ = now;
    HeartbeatMsg hb = {};
    fillHeader(&hb.h, MSG_HEARTBEAT);
    strncpy(hb.fw, VERSION, sizeof(hb.fw) - 1);
    esp_now_send(cfg_->master_mac, reinterpret_cast<uint8_t *>(&hb),
                 sizeof(hb));
  }

  if (!pairing_)
    return;

  if (cfg_->role == ROLE_MASTER &&
      now - lastAnnounceMs_ >= kAnnounceIntervalMs) {
    lastAnnounceMs_ = now;
    AnnounceMsg m = {};
    fillHeader(&m.h, MSG_ANNOUNCE);
    WiFi.softAPmacAddress(m.master_mac);
    m.channel = cfg_->channel;
    strncpy(m.ssid, apSsid().c_str(), sizeof(m.ssid) - 1);
    strncpy(m.car_name, cfg_->car_name, sizeof(m.car_name) - 1);
    esp_now_send(kBroadcast, reinterpret_cast<uint8_t *>(&m), sizeof(m));
  }

  if (now - pairStartMs_ >= kPairTimeoutMs) {
    pairing_ = false;
    printHelper.log("INFO", "pairing timed out");
  }
}

bool EspNowController::peerOnline(int i) const {
  // Heartbeat is every 5 s; allow a couple of misses before "offline".
  return peerSeen_[i] != 0 && (millis() - peerSeen_[i] < 16000);
}

void EspNowController::sendToAllPeers(const uint8_t *buf, size_t len) {
  for (int i = 0; i < kMaxPeers; i++)
    if (cfg_->peers[i].in_use)
      esp_now_send(cfg_->peers[i].mac, buf, len);
}

void EspNowController::savePeerFw() {
  Preferences p;
  p.begin("tyrenow", /*readOnly=*/false);
  p.putBytes("peerfw", peerFw_, sizeof(peerFw_));
  p.end();
}

void EspNowController::sendFwPush() {
  FwPushMsg m = {};
  fillHeader(&m.h, MSG_FW_PUSH);
  sendToAllPeers(reinterpret_cast<uint8_t *>(&m), sizeof(m));
  printHelper.log("INFO", "fw push sent to wheels");
}

void EspNowController::sendStart(uint32_t sessionId, uint16_t rateHz) {
  StartMsg m = {};
  fillHeader(&m.h, MSG_START);
  m.session_id = sessionId;
  m.epoch_ms = 0;
  m.master_millis = millis();
  m.rate_hz = rateHz;
  m.cols = kGridCols;
  m.rows = kGridRows;
  sendToAllPeers(reinterpret_cast<uint8_t *>(&m), sizeof(m));
}

void EspNowController::sendStop(uint32_t sessionId) {
  StopMsg m = {};
  fillHeader(&m.h, MSG_STOP);
  m.session_id = sessionId;
  sendToAllPeers(reinterpret_cast<uint8_t *>(&m), sizeof(m));
}

void EspNowController::sendLiveGrid(uint32_t sessionId, uint32_t tOffsetMs,
                                    const int16_t *temps) {
  if (!cfg_->has_master)
    return;
  LiveGridMsg m = {};
  fillHeader(&m.h, MSG_LIVE_GRID);
  m.session_id = sessionId;
  m.t_offset_ms = tOffsetMs;
  memcpy(m.temps, temps, sizeof(int16_t) * kGridCells);
  esp_now_send(cfg_->master_mac, reinterpret_cast<uint8_t *>(&m), sizeof(m));
}

void EspNowController::recvThunk(const esp_now_recv_info_t *info,
                                 const uint8_t *data, int len) {
  int rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  if (self_)
    self_->onRecv(info->src_addr, data, len, rssi);
}

void EspNowController::onRecv(const uint8_t *srcMac, const uint8_t *data,
                              int len, int rssi) {
  if (len < static_cast<int>(sizeof(MsgHeader)))
    return;
  const MsgHeader *h = reinterpret_cast<const MsgHeader *>(data);

  switch (h->type) {
  case MSG_ANNOUNCE: {
    if (cfg_->role != ROLE_SLAVE || !pairing_)
      return;
    if (len < static_cast<int>(sizeof(AnnounceMsg)))
      return;
    const AnnounceMsg *m = reinterpret_cast<const AnnounceMsg *>(data);
    if (cfg_->group_id != 0) {
      // Car ID set: only pair with a master sharing the same Car ID.
      if (h->group_id != cfg_->group_id)
        return;
    } else {
      // No Car ID: fall back to proximity — adopt the nearest master.
      if (rssi < kPairRssiMin)
        return;
      cfg_->group_id = h->group_id;
    }
    memcpy(cfg_->master_mac, m->master_mac, 6);
    cfg_->channel = m->channel;
    cfg_->has_master = 1;
    strncpy(cfg_->master_ssid, m->ssid, sizeof(cfg_->master_ssid) - 1);
    cfg_->master_ssid[sizeof(cfg_->master_ssid) - 1] = 0;
    strncpy(cfg_->car_name, m->car_name, sizeof(cfg_->car_name) - 1);
    cfg_->car_name[sizeof(cfg_->car_name) - 1] = 0;
    saveConfig(*cfg_);
    addPeer(cfg_->master_mac);

    HelloMsg reply = {};
    fillHeader(&reply.h, MSG_HELLO);
    WiFi.macAddress(reply.mac);
    strncpy(reply.fw, VERSION, sizeof(reply.fw) - 1);
    esp_now_send(cfg_->master_mac, reinterpret_cast<uint8_t *>(&reply),
                 sizeof(reply));
    pairing_ = false;
    printHelper.log("INFO", "paired to master, group=0x%06X", cfg_->group_id);
    break;
  }
  case MSG_HELLO: {
    if (cfg_->role != ROLE_MASTER || !pairing_)
      return;
    if (!accept(h) || len < static_cast<int>(sizeof(HelloMsg)))
      return;
    const HelloMsg *m = reinterpret_cast<const HelloMsg *>(data);
    int slot = -1;
    for (int i = 0; i < kMaxPeers; i++) {
      if (cfg_->peers[i].in_use && !memcmp(cfg_->peers[i].mac, m->mac, 6)) {
        slot = i;
        break;
      }
    }
    if (slot < 0) {
      for (int i = 0; i < kMaxPeers; i++) {
        if (!cfg_->peers[i].in_use) {
          slot = i;
          break;
        }
      }
    }
    if (slot < 0)
      return;  // table full
    memcpy(cfg_->peers[slot].mac, m->mac, 6);
    cfg_->peers[slot].wheel = h->wheel;
    cfg_->peers[slot].in_use = 1;
    strncpy(peerFw_[slot], m->fw, sizeof(peerFw_[slot]) - 1);
    peerFw_[slot][sizeof(peerFw_[slot]) - 1] = 0;
    peerSeen_[slot] = millis();
    saveConfig(*cfg_);
    savePeerFw();
    addPeer(m->mac);
    printHelper.log("INFO", "paired wheel %s", wheelName(h->wheel));
    break;
  }
  case MSG_START: {
    if (!accept(h) || len < static_cast<int>(sizeof(StartMsg)))
      return;
    const StartMsg *m = reinterpret_cast<const StartMsg *>(data);
    if (startCb_)
      startCb_(m->session_id, m->rate_hz);
    break;
  }
  case MSG_STOP: {
    if (!accept(h) || len < static_cast<int>(sizeof(StopMsg)))
      return;
    const StopMsg *m = reinterpret_cast<const StopMsg *>(data);
    if (stopCb_)
      stopCb_(m->session_id);
    break;
  }
  case MSG_LIVE_GRID: {
    if (cfg_->role != ROLE_MASTER || !accept(h))
      return;
    if (len < static_cast<int>(sizeof(LiveGridMsg)))
      return;
    const LiveGridMsg *m = reinterpret_cast<const LiveGridMsg *>(data);
    if (liveCb_)
      liveCb_(h->wheel, m->t_offset_ms, m->temps);
    break;
  }
  case MSG_FW_PUSH: {
    if (cfg_->role != ROLE_SLAVE || !accept(h))
      return;
    if (fwPushCb_)
      fwPushCb_();
    break;
  }
  case MSG_HEARTBEAT: {
    if (cfg_->role != ROLE_MASTER || !accept(h))
      return;
    if (len < static_cast<int>(sizeof(HeartbeatMsg)))
      return;
    const HeartbeatMsg *m = reinterpret_cast<const HeartbeatMsg *>(data);
    for (int i = 0; i < kMaxPeers; i++) {
      if (cfg_->peers[i].in_use && !memcmp(cfg_->peers[i].mac, srcMac, 6)) {
        peerSeen_[i] = millis();
        if (strncmp(peerFw_[i], m->fw, sizeof(peerFw_[i])) != 0) {
          strncpy(peerFw_[i], m->fw, sizeof(peerFw_[i]) - 1);
          peerFw_[i][sizeof(peerFw_[i]) - 1] = 0;
          savePeerFw();
        }
        break;
      }
    }
    break;
  }
  default:
    break;
  }
}

}  // namespace tyre
