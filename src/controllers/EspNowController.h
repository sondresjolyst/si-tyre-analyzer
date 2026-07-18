// Copyright (c) 2026 Sondre Sjølyst
// ESP-NOW coordination between the dash master and the wheel units: button
// pairing (persisted to NVS), session START/STOP sync, and live-grid streaming.
// All messages carry a group_id so neighbouring cars do not cross-trigger.

#ifndef SRC_CONTROLLERS_ESPNOWCONTROLLER_H_
#define SRC_CONTROLLERS_ESPNOWCONTROLLER_H_

#include <esp_now.h>
#include <stdint.h>

#include "config/DeviceConfig.h"

namespace tyre {

enum MsgType : uint8_t {
  MSG_ANNOUNCE = 1,   // master -> broadcast during pairing
  MSG_HELLO = 2,      // slave  -> master, on pair
  MSG_START = 3,      // master -> peers
  MSG_STOP = 4,       // master -> peers
  MSG_LIVE_GRID = 5,  // slave  -> master
  MSG_FW_PUSH = 7,    // master -> peers: pull /fw.bin from the master AP
  MSG_HEARTBEAT = 8,  // slave  -> master: periodic liveness + fw version
  MSG_SYNC = 9,       // master -> peers: upload all stored sessions
  MSG_CONFIG = 10,    // master -> slave: current SSID/car (heartbeat reply)
};

#pragma pack(push, 1)
struct MsgHeader {
  uint8_t type;
  uint8_t wheel;  // sender wheel (WheelPos)
  uint8_t role;   // sender role (Role)
  uint8_t _pad;
  uint32_t group_id;
};

struct AnnounceMsg {
  MsgHeader h;
  uint8_t master_mac[6];
  uint8_t channel;
  char ssid[33];      // master SoftAP name (for post-session upload)
  char car_name[24];  // master's car label, mirrored onto each wheel
};

struct HelloMsg {
  MsgHeader h;
  uint8_t mac[6];
  char fw[16];  // slave firmware version, for the master's version list
};

struct FwPushMsg {
  MsgHeader h;
};

struct SyncMsg {
  MsgHeader h;
};

struct HeartbeatMsg {
  MsgHeader h;
  char fw[16];
};

struct ConfigMsg {
  MsgHeader h;
  char ssid[33];      // master's current SoftAP name
  char car_name[24];  // master's current car label
};

struct StartMsg {
  MsgHeader h;
  uint32_t session_id;
  uint64_t epoch_ms;
  uint32_t master_millis;
  uint16_t rate_hz;
  uint8_t cols;
  uint8_t rows;
  uint8_t opt_lo;
  uint8_t opt_hi;
  char ssid[33];      // master's current SoftAP name, so the wheel's stored
                      // upload target can't drift stale when car_name changes
  char car_name[24];  // master's current car label, mirrored so the wheel's own
                      // SoftAP SSID follows a later master rename
};

struct StopMsg {
  MsgHeader h;
  uint32_t session_id;
};

struct LiveGridMsg {
  MsgHeader h;
  uint32_t session_id;
  uint32_t t_offset_ms;
  int16_t temps[kGridCells];
};
#pragma pack(pop)

class EspNowController {
 public:
  typedef void (*StartCb)(uint32_t sessionId, uint16_t rateHz, uint8_t optLo,
                          uint8_t optHi);
  typedef void (*StopCb)(uint32_t sessionId);
  typedef void (*LiveCb)(uint8_t wheel, uint32_t tOffsetMs,
                         const int16_t *temps);
  typedef void (*FwPushCb)();
  typedef void (*SyncCb)();

  // Init radio (channel from cfg), esp-now, and re-add stored peers.
  // For the master, the SoftAP must already be up on cfg->channel.
  bool begin(DeviceConfig *cfg);

  // Periodic work: master pairing ANNOUNCE + pairing timeout.
  void update();

  void enterPairing();
  void stopPairing() { pairing_ = false; }
  bool pairing() const { return pairing_; }

  // Master: tell peers to start/stop a session.
  void sendStart(uint32_t sessionId, uint16_t rateHz, uint8_t optLo,
                 uint8_t optHi);
  void sendStop(uint32_t sessionId);

  // Slave: stream the latest grid to the master.
  void sendLiveGrid(uint32_t sessionId, uint32_t tOffsetMs,
                    const int16_t *temps);

  // Master: tell paired wheels to pull /fw.bin and self-flash.
  void sendFwPush();

  // Master: tell paired wheels to upload every stored session.
  void sendSyncAll();

  // Last-seen firmware version per peer slot (RAM only, "" until heard).
  const char *peerFw(int i) const { return peerFw_[i]; }

  // True if peer slot i sent a heartbeat recently (i.e. is online/ready).
  bool peerOnline(int i) const;

  void onStart(StartCb cb) { startCb_ = cb; }
  void onStop(StopCb cb) { stopCb_ = cb; }
  void onLive(LiveCb cb) { liveCb_ = cb; }
  void onFwPush(FwPushCb cb) { fwPushCb_ = cb; }
  void onSync(SyncCb cb) { syncCb_ = cb; }

 private:
  static EspNowController *self_;
  static void recvThunk(const esp_now_recv_info_t *info, const uint8_t *data,
                        int len);
  void onRecv(const uint8_t *srcMac, const uint8_t *data, int len, int rssi);

  void addPeer(const uint8_t mac[6]);
  void addStoredPeers();
  void fillHeader(MsgHeader *h, uint8_t type) const;
  bool accept(const MsgHeader *h) const;
  // Slave: adopt the master's current SSID/car label, persisting on change.
  void applyMasterInfo(const char *ssid, const char *carName);
  void sendToAllPeers(const uint8_t *buf, size_t len);  // master fan-out
  void savePeerFw();  // persist peerFw_ to NVS

  DeviceConfig *cfg_ = nullptr;
  uint8_t ifidx_ = 0;  // wifi_interface_t
  bool pairing_ = false;
  uint32_t pairStartMs_ = 0;
  uint32_t lastAnnounceMs_ = 0;
  uint32_t lastHeartbeatMs_ = 0;

  StartCb startCb_ = nullptr;
  StopCb stopCb_ = nullptr;
  LiveCb liveCb_ = nullptr;
  FwPushCb fwPushCb_ = nullptr;
  SyncCb syncCb_ = nullptr;
  char peerFw_[kMaxPeers][16] = {};
  uint32_t peerSeen_[kMaxPeers] = {};  // millis of last heartbeat (0 = never)
};

}  // namespace tyre

#endif  // SRC_CONTROLLERS_ESPNOWCONTROLLER_H_
