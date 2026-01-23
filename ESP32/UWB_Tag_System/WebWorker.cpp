#include "WebWorker.h"
#include "Shared.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h> 

// =================== WIFI CONFIG ===================
static const char *AP_SSID = "UWB_TAG1_ESP32";
static const char *AP_PASS = "12345678";
static const IPAddress AP_IP(192, 168, 88, 1);
static const IPAddress AP_GW(192, 168, 88, 1);
static const IPAddress AP_SN(255, 255, 255, 0);

// **ต้องตรงกับ Router ที่ Tag 2 เกาะอยู่ (ถ้ามี)**
static const char *STA_SSID = "GMR";
static const char *STA_PASS = "12123121211212312121";

WebServer server(80);

// =================== HTML DASHBOARD ===================
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
    --orange: #f97316;
  }
  * { box-sizing: border-box; }
  body { 
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    background: var(--bg); color: var(--text); 
    margin: 0; padding: 16px; 
    display: flex; flex-direction: column; align-items: center; 
    min-height: 100vh;
  }
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
  #map-wrapper { 
    position: relative; width: 100%; aspect-ratio: 16/9; 
    background: radial-gradient(circle at center, #1e1e24 0%, #09090b 100%);
    border-bottom: 1px solid var(--border);
  }
  canvas { display: block; width: 100%; height: 100%; }
  .badge { padding: 4px 8px; border-radius: 6px; font-size: 12px; font-weight: 700; font-family: monospace; }
  .bg-ok { background: rgba(16, 185, 129, 0.2); color: var(--green); border: 1px solid rgba(16, 185, 129, 0.3); }
  .bg-bad { background: rgba(239, 68, 68, 0.2); color: var(--red); border: 1px solid rgba(239, 68, 68, 0.3); }
  .stats-row { display: flex; divide-x: 1px solid var(--border); }
  .stat-item { flex: 1; padding: 16px; text-align: center; border-right: 1px solid var(--border); }
  .stat-item:last-child { border-right: none; }
  .stat-label { font-size: 11px; color: var(--text-dim); margin-bottom: 4px; text-transform: uppercase; }
  .stat-val { font-family: monospace; font-size: 20px; font-weight: 700; color: var(--text); }
  .anchor-list { padding: 16px; display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
  .anchor-item { display: flex; justify-content: space-between; padding-bottom: 8px; border-bottom: 1px solid var(--border); font-family: monospace; font-size: 13px; }
  .an-name { color: var(--text-dim); }
  .an-val { color: var(--accent); font-weight: bold; }
  .controls { padding: 16px; display: flex; gap: 8px; justify-content: center; flex-wrap: wrap; background: #131316; }
  input { 
    background: #000; border: 1px solid var(--border); color: #fff; 
    padding: 8px; border-radius: 8px; width: 80px; text-align: center; font-family: monospace;
  }
  button { 
    background: #27272a; border: 1px solid var(--border); color: #fff; 
    padding: 8px 16px; border-radius: 8px; cursor: pointer; font-weight: 600; font-size: 12px;
  }
  button:hover { background: #3f3f46; border-color: #52525b; }
  .btn-primary { background: var(--accent); border-color: var(--accent); }
  .btn-orange { background: var(--orange); border-color: var(--orange); color: #000; }
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
    <div id="map-wrapper"><canvas id="cvs"></canvas></div>
    <div class="stats-row">
      <div class="stat-item"><div class="stat-label">Tag 1 (Blue)</div><div class="stat-val"><span id="t1_pos">-</span></div></div>
      <div class="stat-item"><div class="stat-label">Tag 2 (Orange)</div><div class="stat-val" style="color:var(--orange)"><span id="t2_pos">-</span></div></div>
      <div class="stat-item"><div class="stat-label">RMSE</div><div class="stat-val" style="color:var(--green)"><span id="rmse">-</span></div></div>
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
      <div class="title" style="color:var(--accent);">Tag 1 Calibration (Local)</div>
      <div id="cal-msg" style="font-size:12px; font-family:monospace; font-weight:bold;">READY</div>
    </div>
    <div class="controls">
      <span style="font-size:12px; color:#aaa; align-self:center;">REF:</span> 
      <input id="cx" value="1.00"><input id="cy" value="1.5">
      <div style="width:1px; height:24px; background:var(--border); margin:0 8px;"></div>
      <button onclick="calp()" class="btn-primary">CAL T1</button>
      <button onclick="save()">SAVE T1</button>
      <button onclick="reset()">RESET T1</button>
    </div>
  </div>
  <div class="card" style="border-color: rgba(249, 115, 22, 0.3);">
    <div class="header" style="background:none; border:none; padding-bottom:0;">
      <div class="title" style="color:var(--orange);">Tag 2 Calibration (Remote)</div>
      <div id="cal2-msg" style="font-size:12px; font-family:monospace; font-weight:bold; color:var(--text-dim);">WAITING</div>
    </div>
    <div class="controls">
      <span style="font-size:12px; color:#aaa; align-self:center;">REF:</span> 
      <input id="cx2" value="1.00"><input id="cy2" value="0.85">
      <div style="width:1px; height:24px; background:var(--border); margin:0 8px;"></div>
      <button onclick="calT2()" class="btn-orange">CAL TAG2 </button>
      <button onclick="saveT2()">SAVE T2</button>
      <button onclick="resetT2()">RESET T2</button>
    </div>
  </div>
</div>
<script>
const cvs = document.getElementById('cvs'), ctx = cvs.getContext('2d');
let FIELD_W = 2.0, FIELD_H = 3.0;
async function api(path){ const r=await fetch(path); return await r.text(); }
function calp(){ 
    api(`/calp?x=${document.getElementById('cx').value}&y=${document.getElementById('cy').value}`); 
    document.getElementById('cal-msg').textContent="SENDING..."; 
}
function save(){ api(`/save`); }
function reset(){ api(`/reset`); }
function calT2(){
    api(`/cal_t2?x=${document.getElementById('cx2').value}&y=${document.getElementById('cy2').value}`);
    document.getElementById('cal2-msg').textContent="CMD SENT...";
}
function saveT2(){ api(`/save_t2`); document.getElementById('cal2-msg').textContent="SAVE CMD SENT..."; }
function resetT2(){ api(`/reset_t2`); document.getElementById('cal2-msg').textContent="RESET CMD SENT..."; }

function drawMap(j) {
  const wrapper = document.getElementById('map-wrapper');
  const dpr = window.devicePixelRatio || 1;
  if(cvs.width !== wrapper.clientWidth * dpr){ cvs.width = wrapper.clientWidth * dpr; cvs.height = wrapper.clientHeight * dpr; ctx.scale(dpr, dpr); }
  const W = wrapper.clientWidth, H = wrapper.clientHeight;
  if(j.field){ FIELD_W = j.field.w; FIELD_H = j.field.h; }
  const pad = 60;
  const sc = Math.min((W - pad*2) / FIELD_W, (H - pad*2) / FIELD_H);
  const ox = (W - (FIELD_W * sc)) / 2, oy = (H - (FIELD_H * sc)) / 2;
  const tx = (x) => ox + (x * sc), ty = (y) => H - oy - (y * sc);

  ctx.clearRect(0,0,W,H);
  // Grid
  ctx.strokeStyle = '#27272a'; ctx.strokeRect(tx(0), ty(FIELD_H), FIELD_W*sc, FIELD_H*sc);
  ctx.beginPath();
  for(let x=0.5; x<FIELD_W; x+=0.5) { ctx.moveTo(tx(x), ty(0)); ctx.lineTo(tx(x), ty(FIELD_H)); }
  for(let y=0.5; y<FIELD_H; y+=0.5) { ctx.moveTo(tx(0), ty(y)); ctx.lineTo(tx(FIELD_W), ty(y)); }
  ctx.setLineDash([4, 4]); ctx.stroke(); ctx.setLineDash([]);

  // Anchors
  if(j.anch_xy) {
    ctx.font = 'bold 11px monospace'; ctx.textAlign = 'center';
    j.anch_xy.forEach((p, i) => {
      const ax = tx(p[0]), ay = ty(p[1]);
      const isOk = (j.a && parseFloat(j.a[i]) > 0);
      ctx.fillStyle = isOk ? '#10b981' : '#3f3f46';
      ctx.beginPath(); ctx.arc(ax, ay, 5, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = isOk ? '#10b981' : '#71717a'; 
      ctx.fillText(`A${i+1} ${j.a?j.a[i]:'-'}m`, ax, (p[1]>FIELD_H/2)?ay-25:ay+25);
    });
  }

  // Draw Robot Vector (Tag1 -> Tag2)
  if ((j.ok || j.x>-0.5) && (j.t2 && j.t2.ok)) {
    const t1x = tx(j.x), t1y = ty(j.y);
    const t2x = tx(j.t2.x), t2y = ty(j.t2.y);
    
    // Line connecting tags
    ctx.beginPath(); 
    ctx.moveTo(t1x, t1y); 
    ctx.lineTo(t2x, t2y); 
    ctx.lineWidth = 4;
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.3)'; 
    ctx.stroke();

    // Direction Arrow (Triangle at center pointing to Tag1)
    // หาจุดกึ่งกลาง
    const cx = (t1x + t2x) / 2;
    const cy = (t1y + t2y) / 2;
    const angle = Math.atan2(t1y - t2y, t1x - t2x); // มุมจาก T2 ไป T1
    
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(angle);
    ctx.beginPath();
    ctx.moveTo(10, 0); // หัวลูกศร
    ctx.lineTo(-5, -5);
    ctx.lineTo(-5, 5);
    ctx.fillStyle = '#ffffff';
    ctx.fill();
    ctx.restore();
  }

  // Tag 1 (Front)
  if(j.ok || j.x>-0.5) {
    const px = tx(j.x), py = ty(j.y);
    ctx.beginPath(); ctx.arc(px, py, 12, 0, Math.PI*2); ctx.strokeStyle='rgba(59,130,246,0.5)'; ctx.stroke();
    ctx.fillStyle='#3b82f6'; ctx.beginPath(); ctx.arc(px, py, 7, 0, Math.PI*2); ctx.fill();
    ctx.fillStyle='#fff'; ctx.fillText('Tag1', px, py-20);
  }
  // Tag 2 (Back)
  if(j.t2 && j.t2.ok) {
    const px = tx(j.t2.x), py = ty(j.t2.y);
    ctx.beginPath(); ctx.arc(px, py, 12, 0, Math.PI*2); ctx.strokeStyle='rgba(249,115,22,0.5)'; ctx.stroke();
    ctx.fillStyle='#f97316'; ctx.beginPath(); ctx.arc(px, py, 7, 0, Math.PI*2); ctx.fill();
    ctx.fillStyle='#fff'; ctx.fillText('Tag2', px, py-20);
  }
}
async function tick(){
  try {
    const r = await fetch('/json'); const j = await r.json();
    document.getElementById('t1_pos').textContent = j.ok ? `${j.x.toFixed(2)}, ${j.y.toFixed(2)}` : "OFFLINE";
    document.getElementById('t2_pos').textContent = (j.t2&&j.t2.ok) ? `${j.t2.x.toFixed(2)}, ${j.t2.y.toFixed(2)}` : "OFFLINE";
    document.getElementById('rmse').textContent = j.rmse.toFixed(3);
    document.getElementById('status-badge').className = j.ok ? "badge bg-ok" : "badge bg-bad";
    document.getElementById('status-badge').textContent = j.ok ? "ONLINE" : "SEARCHING";
    if(j.a) j.a.forEach((v,i)=>document.getElementById('a'+(i+1)).textContent=v+" m");
    const ms=document.getElementById('cal-msg');
    ms.textContent=["READY","CALIBRATING...","SUCCESS!","FAILED","RESET DONE"][j.cs]||"READY";
    ms.style.color=["#aaa","#eab308","#10b981","#ef4444","#3b82f6"][j.cs]||"#aaa";
    drawMap(j);
  } catch(e){}
  setTimeout(tick, 200);
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
  
  bool t2_online = (millis() - t2_last_ms < 3000); 
  json += "\"t2\":{";
  json += "\"ok\":" + String(t2_online ? 1 : 0) + ",";
  json += "\"x\":" + String(t2_x, 2) + ",";
  json += "\"y\":" + String(t2_y, 2);
  json += "},";

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
  cal_state = 1; 
  for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
}

static void handleCalCommon(bool multi) {
  if (!server.hasArg("x")) return;
  float valX = server.arg("x").toFloat();
  float valY = server.arg("y").toFloat();
  if(valX == 0 && valY == 0) { valX = 1.00f; valY = 1.5f; }
  
  calStart(multi, valX, valY);
  server.send(200, "text/plain", "OK");
}

static void handleSave() {
  // [SAFETY] อัปเดตตัวแปรใน RAM ให้เสร็จก่อน
  for (int i = 0; i < 4; i++) {
    if (mp_bias_cnt[i] > 0) {
      bias[i] = mp_bias_sum[i] / mp_bias_cnt[i];
    }
  }
  // เขียนลง Flash (จุดเสี่ยง)
  saveBias(); 
  server.send(200, "text/plain", "Saved");
}

static void handleReset() {
  // [SAFETY] เคลียร์ตัวแปร Runtime ก่อน
  for (int i = 0; i < 4; i++) { 
    bias[i] = 0; 
    mp_bias_sum[i] = 0; 
    mp_bias_cnt[i] = 0;
    
    // เคลียร์ค่าสถานะเก่าออกด้วย เพื่อไม่ให้โชว์ค่าค้าง
    fA[i] = -1;
    lastAccepted[i] = -1;
  }
  
  // เขียนค่าว่างเปล่าลง Flash
  saveBias(); 
  
  cal_state = 4; // State: RESET DONE
  last_rmse = -1; // Reset RMSE
  
  server.send(200, "text/plain", "Reset");
}

// ----------------------------------------------
// TAG 2 COMMAND HANDLERS (ESP-NOW)
// ----------------------------------------------
typedef struct struct_cmd {
  int cmd_id;     // 1=CAL, 2=SAVE, 3=RESET
  float ref_x;
  float ref_y;
} struct_cmd;

void sendCommandToTag2(int cmd, float x, float y) {
  uint8_t tag2_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast
  
  // เช็คก่อนว่ามี Peer นี้หรือยัง
  if (!esp_now_is_peer_exist(tag2_mac)) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, tag2_mac, 6);
    
    // [FIX] ใช้ Channel ปัจจุบันของ WiFi เพื่อให้คุยกันรู้เรื่อง
    peerInfo.channel = WiFi.channel(); 
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("[CMD] Failed to add peer!");
      return; 
    }
  }

  struct_cmd myCmd;
  myCmd.cmd_id = cmd;
  myCmd.ref_x = x;
  myCmd.ref_y = y;
  
  esp_err_t result = esp_now_send(tag2_mac, (uint8_t *) &myCmd, sizeof(myCmd)); 
  if (result == ESP_OK) {
    Serial.printf("[CMD] Sent to Tag 2: Cmd=%d\n", cmd);
  } else {
    Serial.printf("[CMD] Send Error: %d\n", result);
  }
}

static void handleCalT2() {
  float valX = 1.00f; 
  float valY = 0.85f;
  if (server.hasArg("x")) valX = server.arg("x").toFloat();
  if (server.hasArg("y")) valY = server.arg("y").toFloat();
  sendCommandToTag2(1, valX, valY);
  server.send(200, "text/plain", "CMD_SENT");
}

static void handleSaveT2() {
  sendCommandToTag2(2, 0, 0);
  server.send(200, "text/plain", "SAVE_SENT");
}

static void handleResetT2() {
  sendCommandToTag2(3, 0, 0); 
  server.send(200, "text/plain", "RESET_SENT");
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
  
  server.on("/cal_t2", handleCalT2);
  server.on("/save_t2", handleSaveT2);
  server.on("/reset_t2", handleResetT2);
  
  server.begin();
}

void loopWeb() {
  server.handleClient();
}