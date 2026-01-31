#include "WebWorker.h"
#include "Shared.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h> 
// บรรทัดที่ประมาณ 6 ใน UwbWorker.cpp
static bool is_busy_saving = false;
// =================== WIFI CONFIG ===================
static const char *AP_SSID = "UWB_TAG1_ESP32";
static const char *AP_PASS = "12345678";
static const IPAddress AP_IP(192, 168, 88, 1);
static const IPAddress AP_GW(192, 168, 88, 1);
static const IPAddress AP_SN(255, 255, 255, 0);

static const char *STA_SSID = "GMR";
static const char *STA_PASS = "12123121211212312121";

WebServer server(80);

// =================== HTML DASHBOARD (CLEAN VERSION) ===================
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no"/>
<title>UWB Monitor Pro - Multi Tag</title>
<style>
  :root { 
    --bg: #09090b; --card: #18181b; --border: #27272a; 
    --text: #e4e4e7; --text-dim: #a1a1aa;
    --accent: #3b82f6; --accent-glow: rgba(59, 130, 246, 0.15);
    --green: #10b981; --red: #ef4444; --yellow: #eab308;
    --cyan: #00d2ff;
  }
  * { box-sizing: border-box; }
  body { 
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    background: var(--bg); color: var(--text); 
    margin: 0; padding: 16px; 
    display: flex; flex-direction: column; align-items: center; 
    min-height: 100vh;
  }
  .container { width: 100%; max-width: 1080px; display: flex; flex-direction: column; gap: 16px; }
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
      <div class="stat-item"><div class="stat-label">Tag 1 (Front)</div><div class="stat-val"><span id="t1_pos">-</span></div></div>
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
  <div class="header" style="border:none; padding-bottom:0;">
    <div class="title" style="color:var(--accent);">Tag 1 Calibration</div>
    <div id="cal-msg" style="font-size:12px; font-family:monospace; font-weight:bold;">READY</div>
  </div>
  <div class="controls">
    <span style="font-size:12px; color:#aaa; align-self:center;">REF:</span> 
    <input id="cx" type="number" step="0.01" placeholder="X">
    <input id="cy" type="number" step="0.01" placeholder="Y">
    
    <button onclick="calp()" class="btn-primary">CAL T1</button>
    <button onclick="save()">SAVE T1</button>
    <button onclick="reset()">RESET T1</button>
  </div>
</div>

<script>
const cvs = document.getElementById('cvs'), ctx = cvs.getContext('2d');
// บรรทัด 129: แก้ FIELD_W ให้เป็น 3.0 และ FIELD_H เป็น 2.0
let FIELD_W = 3.0, FIELD_H = 2.0; 
const OFFSET_X = 0.0; // ตั้งค่า Offset เป็น 0 เพื่อให้ Tag1 ตรงกับพิกัดจริง

async function api(path){ const r=await fetch(path); return await r.text(); }
function calp(){ api(`/calp?x=${document.getElementById('cx').value}&y=${document.getElementById('cy').value}`); }
function save(){ api(`/save`); }
function reset(){ api(`/reset`); }

// ฟังก์ชันวาดรูปสามเหลี่ยม
function drawTriangle(x, y, size, color, label, direction) {
  ctx.beginPath();
  if(direction === 'left') {
    ctx.moveTo(x - size, y);          // ยอดแหลมไปทางซ้าย
    ctx.lineTo(x + size, y - size);   // มุมขวาบน
    ctx.lineTo(x + size, y + size);   // มุมขวาล่าง
  } else {
    // โค้ดเดิมสำหรับชี้ขึ้น/ลง (ถ้ายังอยากเก็บไว้)
    if(direction === true) {
      ctx.moveTo(x, y - size);
      ctx.lineTo(x - size, y + size);
      ctx.lineTo(x + size, y + size);
    } else {
      ctx.moveTo(x, y + size);
      ctx.lineTo(x - size, y - size);
      ctx.lineTo(x + size, y - size);
    }
  }
  ctx.closePath();
  ctx.fillStyle = color;
  ctx.fill();
  ctx.fillStyle = '#fff';
  ctx.textAlign = 'center';
  ctx.fillText(label, x, y + 35); // ปรับตำแหน่งข้อความให้อยู่ใต้รูป
}

function drawMap(j) {
  const wrapper = document.getElementById('map-wrapper');
  const dpr = window.devicePixelRatio || 1;
  if(cvs.width !== wrapper.clientWidth * dpr){ cvs.width = wrapper.clientWidth * dpr; cvs.height = wrapper.clientHeight * dpr; ctx.scale(dpr, dpr); }
  const W = wrapper.clientWidth, H = wrapper.clientHeight;
  if(j.field){ FIELD_W = j.field.w; FIELD_H = j.field.h; }

  // ระยะเว้นจากขอบ Canvas เข้ามาด้านใน (พิกเซล) ยิ่งน้อยสนามยิ่งใหญ่เต็มจอ
  const pad = 60;

  const sc = Math.min((W - pad*2) / FIELD_W, (H - pad*2) / FIELD_H);
  const ox = (W - (FIELD_W * sc)) / 2, oy = (H - (FIELD_H * sc)) / 2;
  const tx = (x) => ox + (x * sc), ty = (y) => H - oy - (y * sc);

  ctx.clearRect(0,0,W,H);

  // วาดเฉพาะขอบสนาม (ลบ Loop ตารางออกแล้ว)
  ctx.strokeStyle = '#27272a'; 
  ctx.strokeRect(tx(0), ty(FIELD_H), FIELD_W * sc, FIELD_H * sc);

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

 // DRAW LOGIC: แสดงผลเฉพาะ Tag 1 เท่านั้น
if(j.ok || j.x > -0.5) {
  // 1. กำหนดพิกัดหลักจากบอร์ด (Tag 1) และทำการ Clamp ให้อยู่ในขอบเขตสนาม
  const t1_logic_x = Math.max(0, Math.min(j.x, FIELD_W)); 
  const t1_logic_y = Math.max(0, Math.min(j.y, FIELD_H));

  // อัปเดตตัวเลขพิกัดบน Dashboard เฉพาะ Tag 1
  document.getElementById('t1_pos').textContent = j.ok ? `${t1_logic_x.toFixed(2)}, ${t1_logic_y.toFixed(2)}` : "OFFLINE";

  // 2. แปลงพิกัดเมตรเป็นพิกเซลบนจอ
  const t1x = tx(t1_logic_x), t1y = ty(t1_logic_y);

  // 3. วาดรูปสามเหลี่ยมแสดงตำแหน่ง Tag 1 จุดเดียว
  drawTriangle(t1x, t1y, 10, '#3b82f6', 'Tag 1', 'left');
}
}

async function tick(){
  try {
    const r = await fetch('/json'); const j = await r.json();
    const t1_ui_x = j.x - OFFSET_X;
    const t1_ui_y = j.y;
    document.getElementById('t1_pos').textContent =
      j.ok ? `${t1_ui_x.toFixed(2)}, ${t1_ui_y.toFixed(2)}` : "OFFLINE";

    // --- ส่วนที่แก้ไข: การแสดงผล RMSE ---
    const errorM = j.rmse; 
    const errorCm = errorM * 100;
    // แสดงผลเป็น "XX.X cm (X.XXX m)"
    document.getElementById('rmse').textContent = `${errorCm.toFixed(1)} cm (${errorM.toFixed(3)} m)`;
    // ---------------------------------

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
  json += "\"t2\":{\"ok\":" + String(t2_online ? 1 : 0) + ",\"x\":" + String(t2_x, 2) + ",\"y\":" + String(t2_y, 2) + "},";

  json += "\"cs\":" + String(cal_state) + ","; 
  json += "\"a\":[\"" + anchorString(0) + "\",\"" + anchorString(1) + "\",\"" + anchorString(2) + "\",\"" + anchorString(3) + "\"],";
  json += "\"field\":{\"w\":" + String(FIELD_W, 2) + ",\"h\":" + String(FIELD_H, 2) + "},";
  
  // แก้ไขบรรทัดนี้ให้ลำดับ [x,y] ตรงกับ A1, A2, A3, A4
  // A1(0,0), A2(0,2), A3(3,0), A4(3,2)
  json += "\"anch_xy\":[[0.0,0.0],[0.0,2.0],[3.0,0.0],[3.0,2.0]]";
  
  json += "}";
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
  calStart(multi, valX, valY);
  server.send(200, "text/plain", "OK");
}

static void handleSave() {
  for (int i = 0; i < 4; i++) if (mp_bias_cnt[i] > 0) bias[i] = mp_bias_sum[i] / mp_bias_cnt[i];
  saveBias(); 
  server.send(200, "text/plain", "Saved");
}

static void handleReset() {
  for (int i = 0; i < 4; i++) { bias[i] = 0; mp_bias_sum[i] = 0; mp_bias_cnt[i] = 0; fA[i] = -1; lastAccepted[i] = -1; }
  saveBias(); 
  cal_state = 4;
  server.send(200, "text/plain", "Reset");
}

void setupWeb() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(STA_SSID, STA_PASS);
  
  server.on("/", handleIndex);
  server.on("/json", handleJson);
  server.on("/info", handleInfo);
  server.on("/cal", []() { handleCalCommon(false); });
  server.on("/calp", []() { handleCalCommon(true); });
  server.on("/save", handleSave);
  server.on("/reset", handleReset);
  server.begin();
}

void loopWeb() { server.handleClient(); }