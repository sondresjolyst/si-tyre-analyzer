// Copyright (c) 2026 Sondre Sjølyst

#include "web/WebSite.h"

#include <LittleFS.h>

#include <map>

#include "web/Logo.h"

#ifndef VERSION
#define VERSION "v0.0.0"
#endif

namespace tyre {

static const char *kStyle = R"(
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,'Segoe UI',sans-serif;background:#111827;
color:#f3f4f6;min-height:100vh;padding:24px 16px}
.wrap{max-width:560px;margin:0 auto}
h1{font-size:20px;margin-bottom:4px}
h2{font-size:15px;margin:24px 0 8px;color:#9ca3af}
.card{background:#1f2937;border-radius:12px;padding:16px;margin-bottom:16px}
label{display:block;font-size:13px;color:#9ca3af;margin:8px 0 4px}
select,input{width:100%;padding:8px;border-radius:8px;border:1px solid #374151;
background:#111827;color:#f3f4f6}
button{margin-top:12px;width:100%;padding:10px;border:0;border-radius:8px;
background:#2563eb;color:#fff;font-weight:600;cursor:pointer}
table{width:100%;border-collapse:collapse;font-size:13px}
td,th{padding:8px 6px;border-bottom:1px solid #374151;text-align:left}
th{color:#9ca3af;font-weight:600;font-size:12px}
a{color:#60a5fa}
.muted{color:#6b7280;font-size:12px}
.wheel{font-weight:600}.sub{color:#6b7280;font-size:11px}
.hint{color:#6b7280;font-size:11px;margin:6px 0 10px}
.act{text-align:right;white-space:nowrap}
.btn-sm{display:inline-block;padding:5px 10px;border-radius:6px;font-size:12px;
font-weight:600;text-decoration:none;margin-left:6px;border:0;cursor:pointer}
.dl{background:#2563eb;color:#fff}.del{background:#3f1d1d;color:#fca5a5}
.run{margin-bottom:14px}
.runhdr{display:flex;justify-content:space-between;align-items:center;
font-size:13px;color:#9ca3af;margin-bottom:4px}
.runhdr .btn-sm{margin:0;width:auto}
.link{display:block;margin:8px 0}
.logo svg{display:block;width:280px;max-width:78%;height:auto;margin:2px 0}
</style>)";

static String head(const char *title) {
  String h;
  h.reserve(4096);  // grow once instead of reallocating across many appends
  h = "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>";
  h += title;
  h += "</title>";
  h += kStyle;
  h += "</head><body><div class='wrap'>";
  return h;
}

static String opt(const char *value, const char *label, bool sel) {
  String s = "<option value='";
  s += value;
  s += "'";
  if (sel) s += " selected";
  s += ">";
  s += label;
  s += "</option>";
  return s;
}

String pageRoot(const DeviceConfig &cfg) {
  String h = head("SI Tyre Analyzer");
  h += "<div class='logo'>";
  h += kLogo;
  h += "</div><p class='muted'>Version ";
  h += VERSION;
  h += "</p>";

  // --- Live dashboard (master) ---
  if (cfg.role == ROLE_MASTER)
    h += "<h2>Live dashboard</h2><div class='card'>"
         "<a href='/live'><button type='button'>Open live dashboard</button>"
         "</a></div>";

  // --- Config ---
  h += "<h2>Device</h2><div class='card'><form method='POST' action='/config'>";
  h += "<label>Car name</label>"
       "<input type='text' name='car_name' maxlength='23' value='";
  h += cfg.car_name;
  h += "'>";
  h += "<label>Role</label>"
       "<select name='role' id='role' onchange='syncRole()'>";
  h += opt("master", "Dash master", cfg.role == ROLE_MASTER);
  h += opt("slave", "Wheel unit", cfg.role == ROLE_SLAVE);
  h += "</select>";

  h += "<label>Records a wheel?</label>"
       "<select name='has_sensor' id='has_sensor' onchange='syncWheel()'>";
  h += opt("1", "Yes", cfg.has_sensor == 1);
  h += opt("0", "No", cfg.has_sensor == 0);
  h += "</select>";

  h += "<div id='displayRow'><label>Dash display fitted?</label>"
       "<select name='has_display'>";
  h += opt("0", "No", cfg.has_display == 0);
  h += opt("1", "Yes", cfg.has_display == 1);
  h += "</select></div>";

  h += "<div id='wheelRow'><label>Wheel</label><select name='wheel'>";
  h += opt("FL", "Front left", cfg.wheel == WHEEL_FL);
  h += opt("FR", "Front right", cfg.wheel == WHEEL_FR);
  h += opt("RL", "Rear left", cfg.wheel == WHEEL_RL);
  h += opt("RR", "Rear right", cfg.wheel == WHEEL_RR);
  h += "</select></div>";

  h += "<label>ESP-NOW / AP channel</label><select name='channel'>";
  for (int ch : {1, 6, 11}) {
    char v[4];
    snprintf(v, sizeof(v), "%d", ch);
    h += opt(v, v, cfg.channel == ch);
  }
  h += "</select>";

  h += "<button type='submit'>Save</button></form></div>";

  // --- Sessions ---
  h += "<h2>Sessions</h2><div class='card'>"
       "<a href='/sessions'><button type='button'>View sessions</button></a>"
       "</div>";

  // --- Storage ---
  {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    int pct = total ? static_cast<int>((used * 100) / total) : 0;
    h += "<h2>Storage</h2><div class='card'>";
    h += "<p>" + String(used / 1024) + " / " + String(total / 1024) +
         " KB used (" + String(pct) + "%)</p>";
    h += "<div style='background:#374151;border-radius:6px;height:10px'>"
         "<div style='background:#2563eb;height:10px;border-radius:6px;width:" +
         String(pct) + "%'></div></div>";
    h += "<p class='hint'>Oldest sessions are deleted automatically when space "
         "runs low.</p></div>";
  }

  // --- Car ID ---
  h += "<h2>Car ID</h2><div class='card'>";
  h += "<p>Current: <b>";
  h += cfg.group_id ? String(cfg.group_id) : String("not set");
  h += "</b></p>";
  if (cfg.role == ROLE_MASTER) {
    h += "<form method='POST' action='/car/new' onsubmit=\"return confirm("
         "'Generate a new Car ID? Paired wheels will be cleared and must be "
         "re-paired with the new ID.')\">"
         "<button type='submit'>Generate new Car ID</button></form>"
         "<p class='hint'>Enter this number on each wheel unit.</p>";
  } else {
    h += "<form method='POST' action='/car'>"
         "<label>Car ID</label>"
         "<input type='number' name='group_id' min='1000' max='9999' value='";
    h += String(cfg.group_id);
    h += "'><p class='hint'>Enter the Car ID shown on the master.</p>"
         "<button type='submit'>Save</button></form>";
  }
  h += "</div>";

  // --- Pairing ---
  h += "<h2>Pairing</h2><div class='card'>";
  if (cfg.role == ROLE_MASTER) {
    h += "<form method='POST' action='/pair'>"
         "<button type='submit'>Pair wheels (60s)</button></form>"
         "<p class='hint'>Then tap the button on each wheel unit.</p>"
         "<p class='sub' id='paired'>Loading…</p>";
  } else {
    h += "<p class='sub'>";
    if (cfg.has_master) {
      h += "Paired to ";
      h += cfg.master_ssid;
    } else {
      h += "Not paired. Use the master's Pair button, then tap this device.";
    }
    h += "</p>";
  }
  h += "</div>";

  // --- Firmware (one .bin; flash this device, or push to all wheels) ---
  h += "<h2>Firmware</h2><div class='card'>"
       "<form method='POST' action='/update' enctype='multipart/form-data'>"
       "<input type='file' name='firmware' accept='.bin'>"
       "<button type='submit'>Upload &amp; flash this device</button>";
  if (cfg.role == ROLE_MASTER) {
    h += "<button type='submit' formaction='/fw-upload' "
         "formenctype='multipart/form-data' onclick=\"return confirm("
         "'Flash this firmware to all paired wheels?')\">"
         "Upload &amp; flash wheels</button>"
         "<p class='hint'>Wheel build flashes every paired wheel at once.</p>";
  }
  h += "</form></div>";

  h += "<script>function syncWheel(){document.getElementById('wheelRow')"
       ".style.display=document.getElementById('has_sensor').value==='1'"
       "?'':'none';}syncWheel();"
       "function syncRole(){document.getElementById('displayRow')"
       ".style.display=document.getElementById('role').value==='master'"
       "?'':'none';}syncRole();"
       "function pollPeers(){var el=document.getElementById('paired');"
       "if(!el)return;fetch('/api/peers').then(r=>r.json()).then(d=>{"
       "var t='No wheels paired yet.';"
       "if(d.wheels&&d.wheels.length){t='Paired: '+d.wheels.map(function(x){"
       "return x.wheel+' '+(x.fw||'?')+(x.online?' ●':' ○');}).join(', ');}"
       "if(d.pairing)t+='  (pairing…)';el.textContent=t;}).catch(()=>{});}"
       "if(document.getElementById('paired')){pollPeers();setInterval("
       "pollPeers,2000);}</script>";

  h += "</div></body></html>";
  return h;
}

String pageSessions(const std::vector<SessionInfo> &sessions) {
  String h = head("Sessions — SI Tyre Analyzer");
  h += "<h1>Sessions</h1>"
       "<a class='link' href='/'><button type='button'>&larr; Back</button></a>";
  h += "<div class='card'>";
  if (sessions.empty()) {
    h += "<p class='muted'>No sessions recorded yet.</p>";
  } else {
    std::map<uint32_t, std::vector<const SessionInfo *>> runs;
    for (const auto &s : sessions) runs[s.session_id].push_back(&s);

    for (const auto &run : runs) {
      String files;  // JS array for "Download all"
      h += "<div class='run'><div class='runhdr'>Run ";
      h += String(run.first);
      h += "<button type='button' class='btn-sm dl' onclick='dlAll([";
      bool first = true;
      for (const auto *s : run.second) {
        if (!first) files += ",";
        files += "\"" + s->name + "\"";
        first = false;
      }
      h += files;
      h += "])'>Download all</button></div><table>";
      for (const auto *s : run.second) {
        h += "<tr><td class='wheel'>";
        h += wheelName(s->wheel);
        h += "</td><td>";
        h += String(s->records);
        h += " samples</td><td>";
        h += (s->size < 1024) ? (String(s->size) + " B")
                              : (String(s->size / 1024) + " KB");
        h += "</td><td class='act'>"
             "<a class='btn-sm dl' href='/download?file=";
        h += s->name;
        h += "'>Get</a>"
             "<a class='btn-sm del' href='/api/delete?file=";
        h += s->name;
        h += "' onclick=\"return confirm('Delete this file?')\">Del</a>"
             "</td></tr>";
      }
      h += "</table></div>";
    }
  }
  h += "</div>";

  h += "<script>function dlAll(fs){fs.forEach(function(f,i){setTimeout("
       "function(){var a=document.createElement('a');a.href='/download?file='"
       "+f;a.download=f;document.body.appendChild(a);a.click();a.remove();},"
       "i*800);});}</script>";

  h += "</div></body></html>";
  return h;
}

}  // namespace tyre
