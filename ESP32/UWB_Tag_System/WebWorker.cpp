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
// (ผมย่อส่วน HTML ไว้ เพราะส่วนนี้ถูกต้องแล้ว ไม่ต้องแก้)
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no"/>
<title>UWB Monitor Pro</title>
<style>
  :root { --bg:#09090b; --text:#e4e4e7; --accent:#3b82f6; }
  body { font-family:sans-serif; background:var(--bg); color:var(--text); display:flex; flex-direction:column; align-items:center; }
  h1 { color: var(--accent); }
</style>
</head>
<body>
<h1>UWB Local Dashboard</h1>
<p>Please use the React Main Dashboard to view data.</p>
</body>
</html>
)HTML";

// =================== IMPLEMENTATION ===================

IPAddress activeIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP();
  return WiFi.softAPIP();
}

static void handleIndex() { 
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send_P(200, "text/html", INDEX_HTML); 
}

static void handleInfo() { 
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "UWB System Online"); 
}

// ฟังก์ชันส่ง JSON (แก้ CORS ให้แล้ว)
static void handleJson() {
  // ใส่ Header ตรงนี้ เพื่อให้ React เครื่องอื่นดึงได้
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
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
  server.sendHeader("Access-Control-Allow-Origin", "*"); // เพิ่ม CORS
  if (!server.hasArg("x")) return;
  float valX = server.arg("x").toFloat();
  float valY = server.arg("y").toFloat();
  if(valX == 0 && valY == 0) { valX = 1.00f; valY = 1.5f; }
  
  calStart(multi, valX, valY);
  server.send(200, "text/plain", "OK");
}

static void handleSave() {
  server.sendHeader("Access-Control-Allow-Origin", "*"); // เพิ่ม CORS
  // [SAFETY] อัปเดตตัวแปรใน RAM ให้เสร็จก่อน
  for (int i = 0; i < 4; i++) {
    if (mp_bias_cnt[i] > 0) {
      bias[i] = mp_bias_sum[i] / mp_bias_cnt[i];
    }
  }
  // เขียนลง Flash
  saveBias(); 
  server.send(200, "text/plain", "Saved");
}

static void handleReset() {
  server.sendHeader("Access-Control-Allow-Origin", "*"); // เพิ่ม CORS
  // [SAFETY] เคลียร์ตัวแปร Runtime ก่อน
  for (int i = 0; i < 4; i++) { 
    bias[i] = 0; 
    mp_bias_sum[i] = 0; 
    mp_bias_cnt[i] = 0;
    fA[i] = -1;
    lastAccepted[i] = -1;
  }
  
  saveBias(); 
  cal_state = 4; // State: RESET DONE
  last_rmse = -1; 
  
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
  
  if (!esp_now_is_peer_exist(tag2_mac)) {
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, tag2_mac, 6);
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
  server.sendHeader("Access-Control-Allow-Origin", "*");
  float valX = 1.00f; 
  float valY = 0.85f;
  if (server.hasArg("x")) valX = server.arg("x").toFloat();
  if (server.hasArg("y")) valY = server.arg("y").toFloat();
  sendCommandToTag2(1, valX, valY);
  server.send(200, "text/plain", "CMD_SENT");
}

static void handleSaveT2() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  sendCommandToTag2(2, 0, 0);
  server.send(200, "text/plain", "SAVE_SENT");
}

static void handleResetT2() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  sendCommandToTag2(3, 0, 0); 
  server.send(200, "text/plain", "RESET_SENT");
}

// ----------------------------------------------
// NEW: COMMAND HANDLER FOR REACT (STOP/PAUSE)
// ----------------------------------------------
static void handleCommand() {
  // CORS Headers สำคัญมากสำหรับ React
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

  if (server.method() == HTTP_OPTIONS) {
    server.send(204); // OK for pre-flight
    return;
  }

  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    Serial.print("Web Command: "); Serial.println(body);
    
    // ใส่ Logic การควบคุมหุ่นยนต์ตรงนี้
    if (body.indexOf("STOP") >= 0) {
       Serial.println("!!! EMERGENCY STOP !!!");
       // TODO: ใส่โค้ดสั่งหยุดมอเตอร์ที่นี่
    }
    else if (body.indexOf("PAUSE") >= 0) {
       Serial.println(">>> PAUSE ROBOT <<<");
    }
    else if (body.indexOf("RESUME") >= 0) {
       Serial.println(">>> RESUME ROBOT <<<");
    }
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
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
  
  // Register Command Handler (สำคัญ)
  server.on("/cmd", HTTP_ANY, handleCommand);

  server.on("/cal", []() { handleCalCommon(false); });
  server.on("/calp", []() { handleCalCommon(true); });
  server.on("/save", handleSave);
  server.on("/reset", handleReset);
  
  server.on("/cal_t2", handleCalT2);
  server.on("/save_t2", handleSaveT2);
  server.on("/reset_t2", handleResetT2);
  
  server.begin();
  Serial.println("Web Server Started");
}

void loopWeb() {
  server.handleClient();
}