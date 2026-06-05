// Copyright (c) 2026 Sondre Sjølyst

#include "web/WebSite.h"

#include <LittleFS.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

#include "display/Colormap.h"
#include "web/Logo.h"

#ifndef VERSION
#define VERSION "v0.0.0"
#endif

namespace tyre {

static const char *kStyle = R"(
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#111827;--bgd:#0b1220;--surface:#1f2937;--border:#374151;
--txt:#f3f4f6;--dim:#cbd5e1;--muted:#9ca3af;--muted2:#6b7280;
--acc:#2563eb;--acch:#1d4ed8;--blu:#60a5fa;--grn:#22c55e;--red:#f87171}
body{font-family:"Segoe UI",-apple-system,Roboto,system-ui,sans-serif;
color:var(--txt);min-height:100vh;padding:18px 14px 44px;background:var(--bg);
background-image:radial-gradient(130% 90% at 50% -12%,rgba(37,99,235,.10),
transparent 55%);background-attachment:fixed}
.wrap{max-width:580px;margin:0 auto}
.mono{font-family:Consolas,'Cascadia Mono','DejaVu Sans Mono',monospace}
.logo{display:flex;justify-content:center;margin:2px 0 16px}
.logo svg{width:230px;max-width:68%;height:auto}
h1{font-size:18px;letter-spacing:.02em;margin-bottom:4px}
/* status strip */
.bar{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:18px}
.badge{display:inline-flex;align-items:center;gap:6px;
font:600 11px/1 Consolas,'Cascadia Mono',monospace;letter-spacing:.09em;
text-transform:uppercase;padding:7px 10px;border-radius:7px;
background:var(--surface);color:var(--muted);border:1px solid var(--border)}
.badge b{color:var(--txt);letter-spacing:.03em}
.badge.role{background:var(--acc);color:#fff;border-color:var(--acc)}
.badge.car{border-color:var(--blu)}.badge.car b{font-size:14px;color:var(--blu)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--muted2)}
.dot.on{background:var(--grn)}
/* section header */
.sec{font:700 11px/1 Consolas,'Cascadia Mono',monospace;letter-spacing:.14em;
text-transform:uppercase;color:var(--muted);margin:24px 2px 9px;
display:flex;align-items:center;gap:10px}
.sec::after{content:'';flex:1;height:1px;
background:linear-gradient(90deg,var(--border),transparent)}
.card{background:var(--surface);border:1px solid var(--border);
border-radius:14px;padding:16px}
.card+.card{margin-top:12px}
.lbl{display:block;font-size:12px;color:var(--muted);margin:15px 0 7px;
text-transform:uppercase;letter-spacing:.07em}.lbl:first-child{margin-top:0}
.hint{color:var(--muted2);font-size:11px;margin-top:9px;line-height:1.45}
/* inputs */
input[type=text],input[type=number]{width:100%;padding:11px 12px;
border-radius:9px;border:1px solid var(--border);background:var(--bgd);
color:var(--txt);font-size:15px;font-family:inherit}
input[type=text]:focus,input[type=number]:focus{outline:0;border-color:var(--acc)}
input[type=file]{width:100%;font-size:13px;color:var(--muted);padding:11px;
border:1px dashed var(--border);border-radius:9px;background:var(--bgd)}
/* buttons */
.btn{display:block;width:100%;text-align:center;padding:13px;
border-radius:10px;font:700 14px/1.2 inherit;letter-spacing:.05em;
text-transform:uppercase;cursor:pointer;text-decoration:none;
background:transparent;color:var(--txt);border:1px solid var(--border)}
.btn:hover{background:var(--surface)}
.btn+.btn{margin-top:10px}
.btn.primary{background:var(--acc);color:#fff;border-color:var(--acc)}
.btn.primary:hover{background:var(--acch)}
.btn.danger{background:rgba(248,113,113,.12);color:var(--red);
border-color:rgba(248,113,113,.4)}
.btn.lg{padding:18px;font-size:16px;letter-spacing:.08em}
/* segmented control */
.seg{display:flex;border:1px solid var(--border);border-radius:10px;
overflow:hidden;background:var(--bgd)}
.seg input{position:absolute;opacity:0;pointer-events:none}
.seg label{flex:1;text-align:center;padding:12px 6px;font-size:13px;
font-weight:600;color:var(--muted);cursor:pointer;
border-right:1px solid var(--border);transition:background .12s}
.seg label:last-of-type{border-right:0}
.seg input:checked+label{background:var(--acc);color:#fff}
/* wheel 2x2 (car from above) */
.wgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;
max-width:230px;margin:2px auto 0}
.wgrid input{position:absolute;opacity:0;pointer-events:none}
.wgrid label{display:block;text-align:center;padding:17px 0;border-radius:10px;
border:1px solid var(--border);background:var(--bgd);color:var(--muted);
font:700 14px/1 Consolas,'Cascadia Mono',monospace;letter-spacing:.12em;
cursor:pointer}
.wgrid input:checked+label{background:var(--acc);color:#fff;border-color:var(--acc)}
/* storage gauge */
.kv{display:flex;justify-content:space-between;font-size:13px;
color:var(--muted);margin-bottom:9px}
.kv b{color:var(--txt);font-family:Consolas,'Cascadia Mono',monospace}
.gauge{background:var(--bgd);border:1px solid var(--border);border-radius:6px;
height:12px;overflow:hidden}
.gauge>div{height:100%;background:linear-gradient(90deg,var(--grn),var(--acc))}
/* disclosure */
details{margin-top:12px}
summary{list-style:none;cursor:pointer}
summary::-webkit-details-marker{display:none}
.discl{display:flex;align-items:center;justify-content:space-between;
background:var(--surface);border:1px solid var(--border);border-radius:12px;
padding:15px 16px;font:700 12px/1 Consolas,'Cascadia Mono',monospace;
letter-spacing:.12em;text-transform:uppercase}
.discl .chev{color:var(--blu);transition:transform .15s;font-size:14px}
details[open] .discl .chev{transform:rotate(90deg)}
details[open] .discl{border-radius:12px 12px 0 0;border-bottom:0}
details>.card{border-radius:0 0 12px 12px;border-top:0}
/* sessions */
.run{margin-bottom:16px}.run:last-child{margin-bottom:0}
.runhdr{display:flex;justify-content:space-between;align-items:center;
font:600 12px/1 Consolas,'Cascadia Mono',monospace;letter-spacing:.07em;
color:var(--blu);margin-bottom:8px;text-transform:uppercase}
.runhdr .btn-sm{margin:0}
table{width:100%;border-collapse:collapse;font-size:13px}
td,th{padding:9px 6px;border-bottom:1px solid var(--border);text-align:left}
tr:last-child td{border-bottom:0}
.wheel{font-weight:700;font-family:Consolas,'Cascadia Mono',monospace}
.act{text-align:right;white-space:nowrap}
.btn-sm{display:inline-block;padding:6px 11px;border-radius:7px;font-size:12px;
font-weight:700;text-decoration:none;margin-left:6px;border:0;cursor:pointer;
letter-spacing:.04em}
.dl{background:var(--acc);color:#fff}
.del{background:rgba(248,113,113,.14);color:var(--red)}
a{color:var(--blu)}.muted{color:var(--muted);font-size:13px}
.sub{color:var(--muted2);font-size:12px;margin-top:7px}
.paired{color:var(--grn);font-weight:600}
/* live dashboard */
.lgrid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.tyre{background:var(--surface);border:1px solid var(--border);
border-radius:12px;padding:10px}
.tyre .th{display:flex;align-items:center;justify-content:space-between;
font:700 13px/1 Consolas,'Cascadia Mono',monospace;letter-spacing:.1em;
margin-bottom:8px}
.heat{display:grid;gap:0;border-radius:8px;overflow:hidden;
aspect-ratio:.78;width:78%;margin:0 auto;border:1px solid var(--bgd)}
.cell{display:flex;align-items:center;justify-content:center;
font:700 11px/1 Consolas,'Cascadia Mono',monospace;color:#fff;
text-shadow:0 1px 2px rgba(0,0,0,.7)}
.stat{font:600 11px/1 Consolas,'Cascadia Mono',monospace;color:var(--muted);
text-align:center;margin-top:8px;letter-spacing:.02em}
.legend{margin:16px auto 0;max-width:320px}
.legbar{height:12px;border-radius:6px;border:1px solid var(--border);
background:linear-gradient(90deg,rgb(30,70,200) 0%,rgb(30,165,215) 20%,
rgb(40,185,80) 40%,rgb(70,200,70) 60.6%,rgb(200,210,50) 70%,
rgb(240,150,30) 85%,rgb(215,30,30) 100%)}
.leglbl{display:flex;justify-content:space-between;
font:600 11px/1 Consolas,'Cascadia Mono',monospace;color:var(--muted);
margin-top:5px}
</style>)";

static String head(const char *title) {
  String h;
  h.reserve(9000);
  h = "<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>";
  h += title;
  h += "</title>";
  h += kStyle;
  h += "</head><body><div class='wrap'>";
  return h;
}

static String radio(const char *name, const char *id, const char *value,
                    const char *label, bool sel, const char *attr = "") {
  String s = "<input type='radio' name='";
  s += name;
  s += "' id='";
  s += id;
  s += "' value='";
  s += value;
  s += "'";
  if (sel)
    s += " checked";
  if (attr[0]) {
    s += " ";
    s += attr;
  }
  s += "><label for='";
  s += id;
  s += "'>";
  s += label;
  s += "</label>";
  return s;
}

String pageRoot(const DeviceConfig &cfg) {
  const bool master = cfg.role == ROLE_MASTER;
  const String hold = String(kPairHoldMs / 1000);
  String h = head("SI Tyre Analyzer");
  h += "<div class='logo'>";
  h += kLogo;
  h += "</div>";

  h += "<div class='bar'>";
  h += master ? "<span class='badge role'>Dash master</span>"
              : "<span class='badge role'>Wheel unit</span>";
  h += "<span class='badge car'>Car <b>";
  h += cfg.group_id ? String(cfg.group_id) : String("\xE2\x80\x94");
  h += "</b></span>";
  if (cfg.has_sensor) {
    h += "<span class='badge'>Wheel <b>";
    h += wheelName(cfg.wheel);
    h += "</b></span>";
  }
  if (master)
    h += "<span class='badge' id='barPaired'><b>0</b> paired</span>";
  h += "<span class='badge'>";
  h += VERSION;
  h += "</span></div>";

  if (master) {
    h += "<a class='btn primary lg' href='/live'>Open live dashboard</a>";
  } else {
    h += "<div class='card'>";
    if (cfg.has_master) {
      h += "<p class='paired'>\xE2\x97\x8F Paired to ";
      h += cfg.master_ssid;
      h += "</p><p class='sub'>This wheel records when the master starts a "
           "run.</p>";
    } else {
      h += "<p class='muted'>Not paired to a car yet.</p><p class='sub'>On the "
           "master tap Pair wheels, then hold this device's button (~" +
           hold + "s).</p>";
    }
    h += "</div>";
  }
  h += "<a class='btn' href='/sessions'>View recorded sessions</a>";

  h += "<div class='sec'>Pairing</div><div class='card'>";
  if (master) {
    h += "<form method='POST' action='/pair'>"
         "<button class='btn' type='submit'>Pair wheels (60s)</button></form>"
         "<p class='sub' id='paired'>Loading\xE2\x80\xA6</p>";
    h += "<p class='hint'>Tap Pair here, then hold the button (~" + hold +
         "s) on each wheel unit.</p>";
  } else {
    h += "<p class='sub'>";
    if (cfg.has_master) {
      h += "Paired to ";
      h += cfg.master_ssid;
      h += ".";
    } else {
      h += "Not paired. On the master tap Pair, then hold this device's "
           "button (~" +
           hold + "s).";
    }
    h += "</p>";
  }
  h += "</div>";

  h += "<div class='sec'>Car ID</div><div class='card'>";
  if (master) {
    h += "<div class='kv'><span>Current</span><b>";
    h += cfg.group_id ? String(cfg.group_id) : String("not set");
    h += "</b></div>";
    h += "<form method='POST' action='/car/new' onsubmit=\"return confirm("
         "'Generate a new Car ID? Paired wheels will be cleared and must be "
         "re-paired.')\"><button class='btn' type='submit'>"
         "Generate new Car ID</button></form>"
         "<p class='hint'>Enter this number on each wheel unit.</p>";
  } else {
    h += "<form method='POST' action='/car'>"
         "<label class='lbl'>Car ID (shown on the master)</label>"
         "<input type='number' name='group_id' min='1000' max='9999' value='";
    h += String(cfg.group_id);
    h += "'><button class='btn' type='submit' style='margin-top:12px'>Save Car "
         "ID</button></form>";
  }
  h += "</div>";

  h += "<details><summary><div class='discl'>Setup"
       "<span class='chev'>\xE2\x96\xB8</span></div></summary>"
       "<div class='card'><form method='POST' action='/config'>";

  h += "<label class='lbl'>Car name</label>"
       "<input type='text' name='car_name' maxlength='23' value='";
  h += cfg.car_name;
  h += "'>";

  h += "<label class='lbl'>Role</label><div class='seg'>";
  h += radio("role", "rl_m", "master", "Dash master", master,
             "onchange='syncRole()'");
  h += radio("role", "rl_s", "slave", "Wheel unit", !master,
             "onchange='syncRole()'");
  h += "</div>";

  h += "<label class='lbl'>Records a wheel?</label><div class='seg'>";
  h += radio("has_sensor", "hs_1", "1", "Yes", cfg.has_sensor == 1,
             "onchange='syncWheel()'");
  h += radio("has_sensor", "hs_0", "0", "No", cfg.has_sensor == 0,
             "onchange='syncWheel()'");
  h += "</div>";

  h += "<div id='displayRow'><label class='lbl'>Dash display fitted?</label>"
       "<div class='seg'>";
  h += radio("has_display", "hd_0", "0", "No", cfg.has_display == 0);
  h += radio("has_display", "hd_1", "1", "Yes", cfg.has_display == 1);
  h += "</div></div>";

  h += "<div id='wheelRow'><label class='lbl'>Wheel position</label>"
       "<div class='wgrid'>";
  h += radio("wheel", "wl_fl", "FL", "FL", cfg.wheel == WHEEL_FL);
  h += radio("wheel", "wl_fr", "FR", "FR", cfg.wheel == WHEEL_FR);
  h += radio("wheel", "wl_rl", "RL", "RL", cfg.wheel == WHEEL_RL);
  h += radio("wheel", "wl_rr", "RR", "RR", cfg.wheel == WHEEL_RR);
  h += "</div></div>";

  h += "<div id='flipRow'>"
       "<label class='lbl'>Mirror inner/outer (across tread)</label>"
       "<div class='seg'>";
  h += radio("flip_x", "fx_0", "0", "No", cfg.flip_x == 0);
  h += radio("flip_x", "fx_1", "1", "Yes", cfg.flip_x == 1);
  h += "</div><label class='lbl'>Mirror direction of travel</label>"
       "<div class='seg'>";
  h += radio("flip_y", "fy_0", "0", "No", cfg.flip_y == 0);
  h += radio("flip_y", "fy_1", "1", "Yes", cfg.flip_y == 1);
  h += "</div></div>";

  h += "<label class='lbl'>ESP-NOW / AP channel</label><div class='seg'>";
  for (int ch : {1, 6, 11}) {
    char id[8], v[4];
    snprintf(v, sizeof(v), "%d", ch);
    snprintf(id, sizeof(id), "ch_%d", ch);
    h += radio("channel", id, v, v, cfg.channel == ch);
  }
  h += "</div>";

  h += "<label class='lbl'>Optimal window low (\xC2\xB0"
       "C)</label>"
       "<input type='number' name='opt_lo' min='20' max='200' value='";
  h += String(cfg.opt_lo);
  h += "'>";
  h += "<label class='lbl'>Optimal window high (\xC2\xB0"
       "C)</label>"
       "<input type='number' name='opt_hi' min='20' max='200' value='";
  h += String(cfg.opt_hi);
  h += "'>";
  h += "<p class='hint'>Green band = optimal grip. Full scale auto-sets to " +
       String(lroundf(scaleLoC(cfg.opt_lo, cfg.opt_hi))) + "\xE2\x80\x93" +
       String(lroundf(scaleHiC(cfg.opt_lo, cfg.opt_hi))) +
       "\xC2\xB0"
       "C.</p>";

  h += "<button class='btn primary' type='submit' style='margin-top:16px'>"
       "Save setup</button></form></div></details>";

  h += "<details><summary><div class='discl'>Storage &amp; firmware"
       "<span class='chev'>\xE2\x96\xB8</span></div></summary><div "
       "class='card'>";
  {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    int pct = total ? static_cast<int>((used * 100) / total) : 0;
    h += "<div class='kv'><span>Storage used</span><b>" + String(used / 1024) +
         " / " + String(total / 1024) + " KB (" + String(pct) + "%)</b></div>";
    h += "<div class='gauge'><div style='width:" + String(pct) +
         "%'></div></div>"
         "<p class='hint'>Oldest sessions are deleted automatically when space "
         "runs low.</p>";
  }
  h += "<label class='lbl'>Firmware update</label>"
       "<form method='POST' action='/update' enctype='multipart/form-data'>"
       "<input type='file' name='firmware' accept='.bin'>"
       "<button class='btn primary' type='submit' style='margin-top:10px'>"
       "Upload &amp; flash this device</button>";
  if (master) {
    h += "<button class='btn' type='submit' formaction='/fw-upload' "
         "formenctype='multipart/form-data' onclick=\"return confirm("
         "'Flash this firmware to all paired wheels?')\">"
         "Upload &amp; flash all wheels</button>"
         "<p class='hint'>Wheel build flashes every paired wheel at once.</p>";
  }
  h += "</form></div></details>";

  h +=
      "<script>"
      "function syncRole(){document.getElementById('displayRow').style.display="
      "document.getElementById('rl_m').checked?'':'none';}"
      "function syncWheel(){var y=document.getElementById('hs_1').checked;"
      "var d=y?'':'none';document.getElementById('wheelRow').style.display=d;"
      "document.getElementById('flipRow').style.display=d;}"
      "syncRole();syncWheel();"
      "function pollPeers(){var el=document.getElementById('paired');"
      "if(!el)return;fetch('/api/peers').then(r=>r.json()).then(d=>{"
      "var n=d.wheels?d.wheels.length:0;var bp=document.getElementById("
      "'barPaired');if(bp)bp.innerHTML='<b>'+n+'</b> paired';"
      "var t=n?('Paired: '+d.wheels.map(function(x){return x.wheel+' '+"
      "(x.fw||'?')+(x.online?' ●':' ○');}).join(', ')):"
      "'No wheels paired yet.';if(d.pairing)t+='  (pairing…)';"
      "el.textContent=t;}).catch(()=>{});}"
      "if(document.getElementById('paired')){pollPeers();"
      "setInterval(pollPeers,2000);}</script>";

  h += "</div></body></html>";
  return h;
}

String pageSessions(const std::vector<SessionInfo> &sessions) {
  String h = head("Sessions — SI Tyre Analyzer");
  h += "<h1>Recorded sessions</h1>"
       "<a class='btn' href='/' style='margin-bottom:16px'>"
       "\xE2\x86\x90 Back</a>";
  h += "<div class='card'>";
  if (sessions.empty()) {
    h += "<p class='muted'>No sessions recorded yet.</p>";
  } else {
    std::map<uint32_t, std::vector<const SessionInfo *>> runs;
    for (const auto &s : sessions)
      runs[s.session_id].push_back(&s);

    for (const auto &run : runs) {
      String files;
      h += "<div class='run'><div class='runhdr'><span>Run ";
      h += String(run.first);
      h += "</span><button type='button' class='btn-sm dl' onclick='dlAll([";
      bool first = true;
      for (const auto *s : run.second) {
        if (!first)
          files += ",";
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

String pageLive(const DeviceConfig &cfg) {
  const float loEnd = scaleLoC(cfg.opt_lo, cfg.opt_hi);
  const float hiEnd = scaleHiC(cfg.opt_lo, cfg.opt_hi);
  const String lo = String(lroundf(loEnd)), hi = String(lroundf(hiEnd));
  const String mid = String(lroundf((loEnd + hiEnd) / 2.0f));
  String h = head("Live — SI Tyre Analyzer");
  h += "<div class='lgrid'>";
  for (const char *w : {"FL", "FR", "RL", "RR"}) {
    h += "<div class='tyre'><div class='th'><span>";
    h += w;
    h += "</span><span class='dot' id='d";
    h += w;
    h += "'></span></div><div class='heat' id='";
    h += w;
    h += "'></div><div class='stat' id='s";
    h += w;
    h += "'>\xE2\x80\x94</div></div>";
  }
  h += "</div>";
  h += "<div class='legend'><div class='legbar'></div><div class='leglbl'>"
       "<span>" +
       lo + "\xC2\xB0</span><span>" + mid + "\xC2\xB0</span><span>" + hi +
       "\xC2\xB0</span></div></div>";
  h += "<a class='btn' href='/' style='margin-top:18px'>"
       "\xE2\x86\x90 Back</a>";

  h += "<script>var LO=" + lo + ",HI=" + hi + ";";
  h +=
      "var RAMP=[[0,30,70,200],[.2,30,165,215],[.4,40,185,80],"
      "[.606,70,200,70],[.7,200,210,50],[.85,240,150,30],[1,215,30,30]];"
      "function color(t){var f=(t-LO)/(HI-LO);f=Math.max(0,Math.min(1,f));"
      "var i=0;while(i<RAMP.length-2&&f>RAMP[i+1][0])i++;"
      "var a=RAMP[i],b=RAMP[i+1],u=(f-a[0])/(b[0]-a[0]);"
      "return 'rgb('+Math.round(a[1]+(b[1]-a[1])*u)+','"
      "+Math.round(a[2]+(b[2]-a[2])*u)+','+Math.round(a[3]+(b[3]-a[3])*u)+')';}"
      "function draw(id,cols,rows,temps){var el=document.getElementById(id);"
      "if(el.children.length!=cols*rows){el.innerHTML='';"
      "el.style.gridTemplateColumns='repeat('+cols+',1fr)';"
      "for(var i=0;i<cols*rows;i++){var d=document.createElement('div');"
      "d.className='cell';el.appendChild(d);}}"
      "var mid=Math.floor(rows/2);"
      "for(var i=0;i<temps.length;i++){el.children[i].style.background="
      "color(temps[i]);el.children[i].textContent=(Math.floor(i/cols)==mid)?"
      "Math.round(temps[i]):'';}"
      "var mn=Math.min.apply(null,temps),mx=Math.max.apply(null,temps);"
      "var av=temps.reduce(function(a,b){return a+b;},0)/temps.length;"
      "document.getElementById('s'+id).textContent="
      "'min '+Math.round(mn)+'°  avg '+Math.round(av)+"
      "'°  max '+Math.round(mx)+'°';}"
      "function tick(){fetch('/api/live').then(r=>r.json()).then(d=>{"
      "['FL','FR','RL','RR'].forEach(function(w){"
      "var dot=document.getElementById('d'+w);var o=d.wheels[w];"
      "if(o){draw(w,d.cols,d.rows,o.temps);"
      "dot.className='dot'+(o.age_ms<2000?' on':'');}"
      "else{dot.className='dot';}});}).catch(()=>{});}"
      "setInterval(tick,500);tick();</script>";

  h += "</div></body></html>";
  return h;
}

}  // namespace tyre
