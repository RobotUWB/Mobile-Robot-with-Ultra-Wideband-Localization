#include "WebWorker.h"
#include "Shared.h"
#include <WiFi.h>
#include <WebServer.h>

static const char *AP_SSID = "UWB_TAG1_ESP32";
static const char *AP_PASS = "12345678";
static const IPAddress AP_IP(10, 1, 100, 52);
static const IPAddress AP_GW(10, 1, 100, 52);
static const IPAddress AP_SN(255, 255, 255, 0);

static const char *STA_SSID = "GMR";
static const char *STA_PASS = "12123121211212312121";

WebServer server(80);
WiFiClient sseClient;
bool sseConnected = false;

// =================== HTML ===================
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>UWB TAG1 Dashboard</title>
<style>
  body{font-family:system-ui,Segoe UI,Arial;margin:16px;background:#0b0f14;color:#e7eef8}
  .card{background:#121a24;border:1px solid #223049;border-radius:14px;padding:14px;margin-bottom:12px}
  .row{display:flex;gap:12px;flex-wrap:wrap}
  .k{opacity:.75;font-size:12px}
  .v{font-size:22px;font-weight:700}
  .mono{font-family:ui-monospace, SFMono-Regular, Menlo, monospace}
  .ok1{color:#3ddc97} .ok0{color:#ffcc66}
  .bad{color:#ff6b6b}
  table{width:100%;border-collapse:collapse}
  td{padding:6px 0;border-bottom:1px solid #223049}
  a{color:#8ab4ff}
  input{background:#0b0f14;color:#e7eef8;border:1px solid #223049;border-radius:10px;padding:8px}
  button{background:#223049;color:#e7eef8;border:1px solid #334a6a;border-radius:10px;padding:8px 10px;cursor:pointer}
  button:hover{filter:brightness(1.1)}
</style>
</head>
<body>
  <div class="card">
    <div class="k">IP (open in browser)</div>
    <div class="v mono" id="ip">...</div>
    <div class="k mono">
      <a href="/info">/info</a> &nbsp; <a href="/json">/json</a>
    </div>
  </div>

  <div class="row">
    <div class="card" style="flex:1;min-width:260px">
      <div class="k">Position (meters)</div>
      <div class="v mono">x=<span id="x">-</span>  y=<span id="y">-</span></div>
      <div class="k">rmse=<span id="rmse">-</span>  n=<span id="n">-</span>  ok=<span id="ok">-</span></div>
      <div class="k mono">field=<span id="fw">-</span>x<span id="fh">-</span>  tagZ=<span id="tz">-</span></div>
    </div>

    <div class="card" style="flex:1;min-width:260px">
      <div class="k">Quick Calibration (multi-point)</div>
      <div class="mono">x <input id="cx" value="1.50" size="6"/>  y <input id="cy" value="1.00" size="6"/></div>
      <div style="height:8px"></div>
      <button onclick="calp()">CALP</button>
      <button onclick="save()">SAVE</button>
      <button onclick="resetb()">RESET</button>
      <div class="k mono" id="calmsg"></div>
    </div>
  </div>

  <div class="card">
    <div class="k">Anchors (range after bias) & age</div>
    <table class="mono">
      <tr><td>A1</td><td id="a1">-</td><td>age(ms)</td><td id="t1">-</td></tr>
      <tr><td>A2</td><td id="a2">-</td><td>age(ms)</td><td id="t2">-</td></tr>
      <tr><td>A3</td><td id="a3">-</td><td>age(ms)</td><td id="t3">-</td></tr>
      <tr><td>A4</td><td id="a4">-</td><td>age(ms)</td><td id="t4">-</td></tr>
    </table>
    <div class="k mono">Legend: -1=timeout, -2=rejected</div>
  </div>

<script>
async function api(path){
  const r = await fetch(path,{cache:'no-store'});
  return await r.text();
}
async function calp(){
  const x = document.getElementById('cx').value;
  const y = document.getElementById('cy').value;
  const msg = await api(`/calp?x=${encodeURIComponent(x)}&y=${encodeURIComponent(y)}`);
  document.getElementById('calmsg').textContent = msg;
}
async function save(){
  const msg = await api(`/save`);
  document.getElementById('calmsg').textContent = msg;
}
async function resetb(){
  const msg = await api(`/reset`);
  document.getElementById('calmsg').textContent = msg;
}

async function tick(){
  try{
    const r = await fetch('/json',{cache:'no-store'});
    const j = await r.json();
    document.getElementById('ip').textContent = j.ip;

    document.getElementById('x').textContent = (j.x<0?'-':j.x.toFixed(2));
    document.getElementById('y').textContent = (j.y<0?'-':j.y.toFixed(2));
    document.getElementById('rmse').textContent = (j.rmse<0?'-':j.rmse.toFixed(3));
    document.getElementById('n').textContent = j.n;

    document.getElementById('fw').textContent = j.field.w.toFixed(2);
    document.getElementById('fh').textContent = j.field.h.toFixed(2);
    document.getElementById('tz').textContent = j.z.tag.toFixed(2);

    const okEl = document.getElementById('ok');
    okEl.textContent = j.ok;
    okEl.className = (j.ok===1?'ok1':(j.ok===0?'ok0':'bad'));

    ['a1','a2','a3','a4'].forEach((id,i)=>{
      document.getElementById(id).textContent = j.a[i];
      document.getElementById('t'+(i+1)).textContent = j.age_ms[i];
    });
  }catch(e){}
  setTimeout(tick, 250);
}
tick();
</script>
</body></html>
)HTML";

// =================== IMPLEMENTATION ===================
IPAddress activeIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP();
  return WiFi.softAPIP();
}

static void handleIndex() {
  server.send(200, "text/html", INDEX_HTML);
}

static void handleInfo() {
  String s;
  s += "=== UWB TAG1 WEB ===\n";
  s += "STA SSID: " + String(STA_SSID) + "\n";
  s += "STA status: " + String((WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "DISCONNECTED") + "\n";
  s += "STA IP: " + WiFi.localIP().toString() + "\n";
  s += "AP  SSID: " + String(AP_SSID) + "\n";
  s += "AP  IP  : " + WiFi.softAPIP().toString() + "\n";
  s += "AP clients: " + String(WiFi.softAPgetStationNum()) + "\n";
  s += "Field: " + String(FIELD_W, 2) + " x " + String(FIELD_H, 2) + "\n";
  s += "TAG_Z: " + String(TAG_Z, 2) + "\n";
  s += "AZ: [" + String(AZ[0], 2) + "," + String(AZ[1], 2) + "," + String(AZ[2], 2) + "," + String(AZ[3], 2) + "]\n";
  s += "bias: [" + String(bias[0], 3) + "," + String(bias[1], 3) + "," + String(bias[2], 3) + "," + String(bias[3], 3) + "]\n";
  s += "Open: http://" + activeIP().toString() + "/\n";
  server.send(200, "text/plain", s);
}

static void handlePoseSSE() {
  WiFiClient client = server.client();
  client.setNoDelay(true);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/event-stream");
  client.println("Cache-Control: no-cache");
  client.println("Connection: keep-alive");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.println(":ok");
  client.println();

  sseClient = client;
  sseConnected = true;

  Serial.println("[SSE] /pose client connected");
}

static void sendPoseSSE() {
  if (!sseConnected || !sseClient.connected()) {
    sseConnected = false;
    return;
  }

  String json;
  json.reserve(120);
  json += "{\"x_mm\":";
  json += (int32_t)(x_f * 1000);
  json += ",\"y_mm\":";
  json += (int32_t)(y_f * 1000);
  json += ",\"rmse\":";
  json += last_rmse;
  json += ",\"ok\":";
  json += (last_accept ? 1 : 0);
  json += "}";

  sseClient.print("event: pose\n");
  sseClient.print("data: ");
  sseClient.print(json);
  sseClient.print("\n\n");
  sseClient.flush();
}

static void handleJson() {
  uint32_t now = millis();
  uint32_t age[4];
  for (int i = 0; i < 4; i++) {
    if (validRange(fA[i])) age[i] = now - tA[i];
    else age[i] = 999999;
  }

  IPAddress ip = activeIP();

  String json = "{";
  json += "\"ip\":\"" + ip.toString() + "\",";
  json += "\"x\":" + String(have_xy ? x_f : -1.0f, 3) + ",";
  json += "\"y\":" + String(have_xy ? y_f : -1.0f, 3) + ",";
  json += "\"rmse\":" + String(last_rmse, 4) + ",";
  json += "\"n\":" + String(last_n) + ",";
  json += "\"ok\":" + String(last_accept ? 1 : 0) + ",";
  json += "\"a\":[\"" + anchorString(0) + "\",\"" + anchorString(1) + "\",\"" + anchorString(2) + "\",\"" + anchorString(3) + "\"],";
  json += "\"age_ms\":[" + String(age[0]) + "," + String(age[1]) + "," + String(age[2]) + "," + String(age[3]) + "],";
  json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";

  json += "\"field\":{\"w\":" + String(FIELD_W, 2) + ",\"h\":" + String(FIELD_H, 2) + "},";
  json += "\"anch_xy\":["
          "["
          + String(AX[0], 2) + "," + String(AY[0], 2) + "],"
                                                        "["
          + String(AX[1], 2) + "," + String(AY[1], 2) + "],"
                                                        "["
          + String(AX[2], 2) + "," + String(AY[2], 2) + "],"
                                                        "["
          + String(AX[3], 2) + "," + String(AY[3], 2) + "]"
                                                        "],";
  json += "\"bias\":[" + String(bias[0], 3) + "," + String(bias[1], 3) + "," + String(bias[2], 3) + "," + String(bias[3], 3) + "],";
  json += "\"z\":{\"tag\":" + String(TAG_Z, 2) + ",\"anch\":[" + String(AZ[0], 2) + "," + String(AZ[1], 2) + "," + String(AZ[2], 2) + "," + String(AZ[3], 2) + "]}";
  json += "}";

  server.send(200, "application/json", json);
}

static void calStart(bool multi, float x, float y) {
  cal_ref_x = x;
  cal_ref_y = y;
  cal_active = true;
  cal_multi_point = multi;
  cal_start_ms = millis();
  for (int i = 0; i < 4; i++) {
    cal_sum[i] = 0;
    cal_cnt[i] = 0;
  }
}

static void handleCalCommon(bool multi) {
  if (!server.hasArg("x") || !server.hasArg("y")) {
    server.send(400, "text/plain", "usage: /cal?x=1.50&y=1.00  or  /calp?x=1.50&y=1.00");
    return;
  }
  float x = server.arg("x").toFloat();
  float y = server.arg("y").toFloat();
  calStart(multi, x, y);
  server.send(200, "text/plain", String(multi ? "CALP" : "CAL") + " started ref=(" + String(x, 2) + "," + String(y, 2) + ") for " + String(CAL_MS) + "ms");
}

static void handleSave() {
  for (int i = 0; i < 4; i++) {
    if (mp_bias_cnt[i] > 0) bias[i] = (float)(mp_bias_sum[i] / (double)mp_bias_cnt[i]);
  }
  saveBias();
  server.send(200, "text/plain",
              "saved bias: [" + String(bias[0], 3) + "," + String(bias[1], 3) + "," + String(bias[2], 3) + "," + String(bias[3], 3) + "]");
}

static void handleReset() {
  for (int i = 0; i < 4; i++) {
    bias[i] = 0;
    mp_bias_sum[i] = 0;
    mp_bias_cnt[i] = 0;
  }
  saveBias();
  server.send(200, "text/plain", "reset bias=0 and saved");
}

static void wifiStart_AP_STA() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
  bool ap_ok = WiFi.softAP(AP_SSID, AP_PASS);
  delay(200);

  WiFi.begin(STA_SSID, STA_PASS);
  Serial.printf("Connecting STA to '%s' ...\n", STA_SSID);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("=== WiFi Status ===");
  Serial.printf("AP  ok? %s\n", ap_ok ? "YES" : "NO");
  Serial.printf("AP  SSID: %s\n", AP_SSID);
  Serial.print("AP  IP  : ");
  Serial.println(WiFi.softAPIP());

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("STA SSID: %s\n", WiFi.SSID().c_str());
    Serial.print("STA IP  : ");
    Serial.println(WiFi.localIP());
    Serial.print("Open dashboard (GMR): http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
  } else {
    Serial.println("STA connect FAILED -> use AP fallback");
    Serial.print("Open dashboard (AP): http://");
    Serial.print(WiFi.softAPIP());
    Serial.println("/");
  }
}

// =================== PUBLIC SETUP/LOOP ===================
void setupWeb() {
  wifiStart_AP_STA();

  server.on("/", handleIndex);
  server.on("/json", handleJson);
  server.on("/info", handleInfo);
  server.on("/pose", HTTP_GET, handlePoseSSE);

  server.on("/cal", []() {
    handleCalCommon(false);
  });
  server.on("/calp", []() {
    handleCalCommon(true);
  });
  server.on("/save", handleSave);
  server.on("/reset", handleReset);

  server.begin();
  Serial.println("Web server started.");
}

void loopWeb() {
  server.handleClient();
  static uint32_t lastSSE = 0;
  if (millis() - lastSSE >= 50) {   // 20Hz
    lastSSE = millis();
    sendPoseSSE();
  }
}