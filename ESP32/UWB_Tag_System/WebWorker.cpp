#include "WebWorker.h"
#include "Shared.h"
#include <WiFi.h>
#include <WebServer.h>

// =================== WIFI CONFIG ===================
static const char *AP_SSID = "UWB_TAG1_ESP32";
static const char *AP_PASS = "12345678";
static const IPAddress AP_IP(192, 168, 88, 1);
static const IPAddress AP_GW(192, 168, 88, 1);
static const IPAddress AP_SN(255, 255, 255, 0);

static const char *STA_SSID = "GMR";
static const char *STA_PASS = "12123121211212312121";

WebServer server(80);

// =================== HTML DASHBOARD (PRO + HEX ID) ===================
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no"/>
<title>UWB Monitor Pro</title>
<style>
  :root { 
    --bg: #09090b; --card: #18181b; --border: #27272a; 
    --text: #e4e4e7; --text-dim: #a1a1aa;
    --accent: #3b82f6; --accent-glow: rgba(59, 130, 246, 0.15);
    --green: #10b981; --red: #ef4444; --yellow: #eab308;
  }
  
  * { box-sizing: border-box; }
  body { 
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji", "Segoe UI Symbol";
    background: var(--bg); color: var(--text); 
    margin: 0; padding: 16px; 
    display: flex; flex-direction: column; align-items: center; 
    min-height: 100vh;
  }
  
  /* Layout */
  .container { width: 100%; max-width: 800px; display: flex; flex-direction: column; gap: 16px; }
  
  .card { 
    background: var(--card); border: 1px solid var(--border); 
    border-radius: 16px; overflow: hidden;
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
  }
  
  .header {
    padding: 12px 16px; border-bottom: 1px solid var(--border);
    display: flex; justify-content: space-between; align-items: center;
    background: rgba(255,255,255,0.02);
  }
  .title { font-size: 14px; font-weight: 600; letter-spacing: 0.5px; color: var(--text-dim); text-transform: uppercase; }
  
  /* Map Canvas Wrapper */
  #map-wrapper { 
    position: relative; width: 100%; aspect-ratio: 16/9; 
    background: radial-gradient(circle at center, #1e1e24 0%, #09090b 100%);
    border-bottom: 1px solid var(--border);
  }
  canvas { display: block; width: 100%; height: 100%; }
  
  /* Floating Badge */
  .badge { 
    padding: 4px 8px; border-radius: 6px; font-size: 12px; font-weight: 700; font-family: monospace;
  }
  .bg-ok { background: rgba(16, 185, 129, 0.2); color: var(--green); border: 1px solid rgba(16, 185, 129, 0.3); }
  .bg-bad { background: rgba(239, 68, 68, 0.2); color: var(--red); border: 1px solid rgba(239, 68, 68, 0.3); }

  /* Data Display Row */
  .stats-row { 
    display: flex; divide-x: 1px solid var(--border); 
  }
  .stat-item { 
    flex: 1; padding: 16px; text-align: center; 
    border-right: 1px solid var(--border);
  }
  .stat-item:last-child { border-right: none; }
  
  .stat-label { font-size: 11px; color: var(--text-dim); margin-bottom: 4px; text-transform: uppercase; }
  .stat-val { font-family: monospace; font-size: 24px; font-weight: 700; color: var(--text); }
  .unit { font-size: 14px; color: var(--text-dim); font-weight: normal; }

  /* New Anchor List Style */
  .anchor-list { padding: 16px; display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
  .anchor-item { display: flex; justify-content: space-between; padding-bottom: 8px; border-bottom: 1px solid var(--border); font-family: monospace; font-size: 13px; }
  .an-name { color: var(--text-dim); }
  .an-val { color: var(--accent); font-weight: bold; }

  /* Controls */
  .controls { padding: 16px; display: flex; gap: 8px; justify-content: center; flex-wrap: wrap; background: #131316; }
  input { 
    background: #000; border: 1px solid var(--border); color: #fff; 
    padding: 8px; border-radius: 8px; width: 70px; text-align: center; font-family: monospace;
  }
  button { 
    background: #27272a; border: 1px solid var(--border); color: #fff; 
    padding: 8px 16px; border-radius: 8px; cursor: pointer; font-weight: 600; font-size: 12px;
    transition: all 0.2s;
  }
  button:hover { background: #3f3f46; border-color: #52525b; }
  button:active { transform: translateY(1px); }
  .btn-primary { background: var(--accent); border-color: var(--accent); }
  .btn-primary:hover { background: #2563eb; }

  /* Calibration Message Animation */
  @keyframes blink { 50% { opacity: 0.5; } }
  .blink { animation: blink 1s infinite; }

</style>
</head>
<body>

<div class="container">
  
  <div class="card">
    <div class="header">
      <div class="title">Live Position Tracking</div>
      <div id="status-badge" class="badge bg-bad">OFFLINE</div>
    </div>
    
    <div id="map-wrapper">
      <canvas id="cvs"></canvas>
    </div>

    <div class="stats-row">
      <div class="stat-item">
        <div class="stat-label">Position X</div>
        <div class="stat-val"><span id="x">-</span><span class="unit">m</span></div>
      </div>
      <div class="stat-item">
        <div class="stat-label">Position Y</div>
        <div class="stat-val"><span id="y">-</span><span class="unit">m</span></div>
      </div>
      <div class="stat-item">
        <div class="stat-label">Precision (RMSE)</div>
        <div class="stat-val" style="color:var(--green)"><span id="rmse">-</span></div>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="header"><div class="title">Anchor Status</div></div>
    <div class="anchor-list">
      <div class="anchor-item"><span class="an-name">A1 (0x84)</span> <span id="a1" class="an-val">-</span></div>
      <div class="anchor-item"><span class="an-name">A2 (0x85)</span> <span id="a2" class="an-val">-</span></div>
      <div class="anchor-item"><span class="an-name">A3 (0x86)</span> <span id="a3" class="an-val">-</span></div>
      <div class="anchor-item"><span class="an-name">A4 (0x87)</span> <span id="a4" class="an-val">-</span></div>
    </div>
  </div>

  <div class="card">
    <div class="header" style="background:none; border:none; padding-bottom:0;">
      <div class="title">System Calibration</div>
      <div id="cal-msg" style="font-size:12px; font-family:monospace; font-weight:bold;">READY</div>
    </div>
    <div class="controls">
      <div style="display:flex; align-items:center; gap:8px;">
        <span style="font-size:12px; color:#aaa;">REF X:</span> <input id="cx" value="1.50">
        <span style="font-size:12px; color:#aaa;">REF Y:</span> <input id="cy" value="1.00">
      </div>
      <div style="width:1px; height:24px; background:var(--border); margin:0 8px;"></div>
      <button onclick="calp()" class="btn-primary">CAL (POINT)</button>
      <button onclick="save()">SAVE</button>
      <button onclick="reset()">RESET</button>
    </div>
  </div>

</div>

<script>
const cvs = document.getElementById('cvs');
const ctx = cvs.getContext('2d');

let FIELD_W = 3.0; 
let FIELD_H = 2.0;

const ANCHOR_HEX = ["0x84", "0x85", "0x86", "0x87"];

// API Helpers
async function api(path){ const r=await fetch(path); return await r.text(); }

// Actions
function calp(){
  const x=document.getElementById('cx').value, y=document.getElementById('cy').value;
  api(`/calp?x=${x}&y=${y}`);
  document.getElementById('cal-msg').textContent = "SENDING...";
}
function save(){ api(`/save`); }
function reset(){ api(`/reset`); }

// --- DRAWING ENGINE (Optimized) ---
function drawMap(j) {
  const wrapper = document.getElementById('map-wrapper');
  const rect = wrapper.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  
  if(cvs.width !== rect.width * dpr || cvs.height !== rect.height * dpr){
    cvs.width = rect.width * dpr; cvs.height = rect.height * dpr;
    ctx.scale(dpr, dpr);
  }
  const W = rect.width; const H = rect.height;

  if(j.field){ FIELD_W = j.field.w; FIELD_H = j.field.h; }
  
  const pad = 60;
  const scaleX = (W - pad*2) / FIELD_W;
  const scaleY = (H - pad*2) / FIELD_H;
  const sc = Math.min(scaleX, scaleY);
  
  const offsetX = (W - (FIELD_W * sc)) / 2;
  const offsetY = (H - (FIELD_H * sc)) / 2;
  const tx = (x) => offsetX + (x * sc);
  const ty = (y) => H - offsetY - (y * sc);

  ctx.clearRect(0,0,W,H);
  
  // Grid
  ctx.strokeStyle = '#27272a'; ctx.lineWidth = 1;
  ctx.strokeRect(tx(0), ty(FIELD_H), FIELD_W*sc, FIELD_H*sc);
  
  ctx.beginPath();
  for(let x=0.5; x<FIELD_W; x+=0.5) { ctx.moveTo(tx(x), ty(0)); ctx.lineTo(tx(x), ty(FIELD_H)); }
  for(let y=0.5; y<FIELD_H; y+=0.5) { ctx.moveTo(tx(0), ty(y)); ctx.lineTo(tx(FIELD_W), ty(y)); }
  ctx.setLineDash([4, 4]); ctx.stroke(); ctx.setLineDash([]);

  // Anchors
  if(j.anch_xy) {
    ctx.font = 'bold 11px monospace'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    j.anch_xy.forEach((p, i) => {
      const ax = tx(p[0]); const ay = ty(p[1]);
      const distStr = (j.a && j.a[i]) ? j.a[i] : "-";
      const distVal = parseFloat(distStr);
      const isOk = (distVal > 0);
      const isTop = (p[1] > FIELD_H/2);
      const labelY = isTop ? ay - 25 : ay + 25;
      const labelText = `A${i+1}(${ANCHOR_HEX[i]}) ${distVal.toFixed(2)}m`;
      const textW = ctx.measureText(labelText).width + 12;
      
      ctx.fillStyle = isOk ? 'rgba(16, 185, 129, 0.15)' : 'rgba(39, 39, 42, 0.8)';
      ctx.strokeStyle = isOk ? '#10b981' : '#52525b';
      ctx.lineWidth = 1;
      
      roundRect(ctx, ax - textW/2, labelY - 10, textW, 20, 6);
      ctx.fill(); ctx.stroke();
      
      ctx.fillStyle = isOk ? '#10b981' : '#71717a'; ctx.fillText(labelText, ax, labelY);
      ctx.shadowBlur = 10; ctx.shadowColor = isOk ? '#10b981' : 'transparent';
      ctx.fillStyle = isOk ? '#10b981' : '#3f3f46';
      ctx.beginPath(); ctx.arc(ax, ay, 5, 0, Math.PI*2); ctx.fill(); ctx.shadowBlur = 0;
    });
  }

  // Tag
  if(j.ok === 1 || j.x > -0.5) {
    const tx_pos = tx(j.x); const ty_pos = ty(j.y);
    ctx.strokeStyle = 'rgba(59, 130, 246, 0.4)'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(tx_pos, ty_pos, 12 + Math.sin(Date.now()/200)*2, 0, Math.PI*2); ctx.stroke();
    ctx.fillStyle = '#3b82f6'; ctx.shadowBlur = 15; ctx.shadowColor = '#3b82f6';
    ctx.beginPath(); ctx.arc(tx_pos, ty_pos, 7, 0, Math.PI*2); ctx.fill(); ctx.shadowBlur = 0;
    ctx.fillStyle = '#fff'; ctx.font = '12px monospace';
    ctx.fillText(`(${j.x.toFixed(2)}, ${j.y.toFixed(2)})`, tx_pos, ty_pos - 22);
  }
}

function roundRect(ctx, x, y, w, h, r) {
  if (w < 2 * r) r = w / 2; if (h < 2 * r) r = h / 2;
  ctx.beginPath(); ctx.moveTo(x + r, y); ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r); ctx.arcTo(x, y + h, x, y, r); ctx.arcTo(x, y, x + w, y, r); ctx.closePath();
}

// --- STATUS LOGIC ---
// 0=Ready, 1=Calibrating, 2=Success, 4=Reset
const ST_TXT = ["READY", "CALIBRATING (WAIT 5s)...", "SUCCESS! (PRESS SAVE)", "FAILED", "RESET DONE"];
const ST_COL = ["#aaa", "#eab308", "#10b981", "#ef4444", "#3b82f6"];

async function tick(){
  try {
    const r = await fetch('/json');
    const j = await r.json();
    
    document.getElementById('x').textContent = j.x.toFixed(2);
    document.getElementById('y').textContent = j.y.toFixed(2);
    document.getElementById('rmse').textContent = j.rmse.toFixed(3);
    
    const badge = document.getElementById('status-badge');
    if(j.ok) { badge.textContent = "ONLINE"; badge.className = "badge bg-ok"; }
    else { badge.textContent = "SEARCHING"; badge.className = "badge bg-bad"; }

    j.a.forEach((val, i) => { const el=document.getElementById('a'+(i+1)); if(el) el.textContent=val+" m"; });

    // >>> UPDATE CALIBRATION MESSAGE <<<
    const cs = j.cs || 0; 
    const msgEl = document.getElementById('cal-msg');
    
    // Check if text is manually set to sending to avoid flicker, else update from server
    if(msgEl.textContent !== "SENDING...") {
        msgEl.textContent = ST_TXT[cs] || "READY";
        msgEl.style.color = ST_COL[cs] || "#aaa";
        if(cs === 1) msgEl.classList.add('blink'); else msgEl.classList.remove('blink');
    }

    drawMap(j);
  } catch(e) {}
  setTimeout(tick, 100);
}
tick();
</script>
</body>
</html>
)HTML";

// =================== IMPLEMENTATION ===================
IPAddress activeIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP();
  return WiFi.softAPIP();
}

static void handleIndex() { 
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send_P(200, "text/html", INDEX_HTML); 
}

static void handleInfo() { server.send(200, "text/plain", "UWB System Online"); }

static void handleJson() {
  String json = "{";
  json += "\"x\":" + String(have_xy ? x_f : -1.0f, 3) + ",";
  json += "\"y\":" + String(have_xy ? y_f : -1.0f, 3) + ",";
  json += "\"rmse\":" + String(last_rmse, 4) + ",";
  json += "\"n\":" + String(last_n) + ",";
  json += "\"ok\":" + String(last_accept ? 1 : 0) + ",";
  
  // >>> Send Status State to Frontend
  json += "\"cs\":" + String(cal_state) + ","; 
  
  json += "\"a\":[\"" + anchorString(0) + "\",\"" + anchorString(1) + "\",\"" + anchorString(2) + "\",\"" + anchorString(3) + "\"],";
  json += "\"field\":{\"w\":" + String(FIELD_W, 2) + ",\"h\":" + String(FIELD_H, 2) + "},";
  
  json += "\"anch_xy\":[";
  for(int i=0; i<4; i++) {
    json += "[" + String(AX[i], 2) + "," + String(AY[i], 2) + "]";
    if(i<3) json += ",";
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

static void calStart(bool multi, float x, float y) {
  cal_ref_x = x; cal_ref_y = y; cal_active = true; cal_multi_point = multi;
  cal_start_ms = millis();
  // >>> SET STATE = 1 (Calibrating)
  cal_state = 1; 
  for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
}

static void handleCalCommon(bool multi) {
  if (!server.hasArg("x")) return;
  calStart(multi, server.arg("x").toFloat(), server.arg("y").toFloat());
  server.send(200, "text/plain", "OK");
}

static void handleSave() {
  for (int i = 0; i < 4; i++) if (mp_bias_cnt[i] > 0) bias[i] = mp_bias_sum[i]/mp_bias_cnt[i];
  saveBias(); server.send(200, "text/plain", "Saved");
}

static void handleReset() {
  for (int i = 0; i < 4; i++) { bias[i] = 0; mp_bias_sum[i] = 0; mp_bias_cnt[i] = 0; }
  saveBias(); 
  // >>> SET STATE = 4 (Reset Done)
  cal_state = 4;
  server.send(200, "text/plain", "Reset");
}

static void wifiStart_AP_STA() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(STA_SSID, STA_PASS);
}

void setupWeb() {
  wifiStart_AP_STA();
  server.on("/", handleIndex);
  server.on("/json", handleJson);
  server.on("/info", handleInfo);
  server.on("/cal", []() { handleCalCommon(false); });
  server.on("/calp", []() { handleCalCommon(true); });
  server.on("/save", handleSave);
  server.on("/reset", handleReset);
  server.begin();
}

void loopWeb() {
  server.handleClient();
}