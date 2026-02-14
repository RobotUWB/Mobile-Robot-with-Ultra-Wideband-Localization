#include "WebWorker.h"
#include "Shared.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h> 

// [Brownout Fix]
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ประกาศ WebSocket ที่ Port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// WebServer ที่ Port 80 (ยังเก็บไว้เป็น Backup)
WebServer server(80);

// ตัวแปร Buffer ขนาดใหญ่
char msgBuffer[1024]; 

// =================== WIFI CONFIG ===================
static const char *AP_SSID = "UWB_TAG1_ESP32";
static const char *AP_PASS = "12345678";

static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GW(192, 168, 4, 1);
static const IPAddress AP_SN(255, 255, 255, 0);

static const char *STA_SSID = "GMR";
static const char *STA_PASS = "12123121211212312121";

static const IPAddress STA_FIX_IP(192, 168, 88, 99);   
static const IPAddress STA_FIX_GW(192, 168, 88, 1);    
static const IPAddress STA_FIX_SN(255, 255, 255, 0);   

// ฟังก์ชันสร้าง JSON (Optimized)
char* buildJson() {
  bool t2_online = (millis() - t2_last_ms < 3000); 

  // ดึงค่าอย่างปลอดภัย (Thread Safe)
  float safe_x, safe_y, safe_rmse;
  int safe_n, safe_state;
  bool safe_ok;
  
  portENTER_CRITICAL(&myMutex);
  safe_x = have_xy ? x_f : -1.0f;
  safe_y = have_xy ? y_f : -1.0f;
  safe_rmse = last_rmse;
  safe_n = last_n;
  safe_ok = last_accept;
  safe_state = cal_state;
  portEXIT_CRITICAL(&myMutex);

  snprintf(msgBuffer, sizeof(msgBuffer), 
    "{"
    "\"x\":%.3f,\"y\":%.3f,\"rmse\":%.4f,\"n\":%d,\"ok\":%d,"
    "\"t2\":{\"ok\":%d,\"x\":%.2f,\"y\":%.2f},"
    "\"cs\":%d,"
    "\"a\":[\"%s\",\"%s\",\"%s\",\"%s\"],"
    "\"field\":{\"w\":%.2f,\"h\":%.2f},"
    "\"anch_xy\":[[0.0,0.0],[0.0,2.0],[3.0,0.0],[3.0,2.0]]"
    "}",
    safe_x, safe_y, safe_rmse, safe_n, safe_ok ? 1 : 0,
    t2_online ? 1 : 0, t2_x, t2_y,
    safe_state,
    anchorString(0).c_str(), anchorString(1).c_str(), anchorString(2).c_str(), anchorString(3).c_str(),
    FIELD_W, FIELD_H
  );

  return msgBuffer;
}

// =================== LOGIC HELPERS ===================

// เริ่มต้น Calibration
static void calStart(bool multi, float x, float y) {
  cal_ref_x = x; cal_ref_y = y; cal_active = true; cal_multi_point = multi;
  cal_start_ms = millis();
  cal_state = 1; 
  for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
  Serial.printf("[WS/HTTP] Cal Start: x=%.2f y=%.2f multi=%d\n", x, y, multi);
}

// =================== HTTP HANDLERS (Backup) ===================

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
  Serial.println("[WS/HTTP] Saved Bias");
}

static void handleReset() {
  for (int i = 0; i < 4; i++) { bias[i] = 0; mp_bias_sum[i] = 0; mp_bias_cnt[i] = 0; fA[i] = -1; lastAccepted[i] = -1; }
  saveBias(); 
  cal_state = 4;
  server.send(200, "text/plain", "Reset");
  Serial.println("[WS/HTTP] Reset Bias");
}

static void handleInfo() { server.send(200, "text/plain", "UWB System Online"); }
static void handleJson() { server.send(200, "application/json", buildJson()); }

// =================== WEBSOCKET SETUP & EVENTS ===================

IPAddress activeIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP();
  return WiFi.softAPIP();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      break;
    case WStype_CONNECTED:
      {
        webSocket.sendTXT(num, buildJson()); 
      }
      break;
    case WStype_TEXT:
      {
        // [เพิ่ม] รับคำสั่งผ่าน WebSocket (CAL, SAVE, RESET)
        String text = String((char *)payload);
        text.trim(); // ตัดช่องว่างหัวท้าย
        
        // 1. คำสั่ง CAL (จุดเดียว) -> ส่งมาว่า "CAL 1.5 1.0"
        if (text.startsWith("CAL ")) {
           float valX = 0, valY = 0;
           if (sscanf(text.c_str(), "CAL %f %f", &valX, &valY) == 2) {
               calStart(false, valX, valY);
           }
        }
        // 2. คำสั่ง CALP (หลายจุด) -> ส่งมาว่า "CALP 1.5 1.0"
        else if (text.startsWith("CALP")) {
           float valX = 0, valY = 0;
           if (sscanf(text.c_str(), "CALP %f %f", &valX, &valY) == 2) {
               calStart(true, valX, valY);
           }
        }
        // 3. คำสั่ง SAVE -> ส่งมาว่า "SAVE"
        else if (text == "SAVE") {
           // Logic เดียวกับ handleSave
           for (int i = 0; i < 4; i++) if (mp_bias_cnt[i] > 0) bias[i] = mp_bias_sum[i] / mp_bias_cnt[i];
           saveBias();
        }
        // 4. คำสั่ง RESET -> ส่งมาว่า "RESET"
        else if (text == "RESET") {
           // Logic เดียวกับ handleReset
           for (int i = 0; i < 4; i++) { bias[i] = 0; mp_bias_sum[i] = 0; mp_bias_cnt[i] = 0; fA[i] = -1; lastAccepted[i] = -1; }
           saveBias(); 
           cal_state = 4;
        }
      }
      break;
  }
}

void setupWeb() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
  WiFi.softAP(AP_SSID, AP_PASS);

  if (!WiFi.config(STA_FIX_IP, STA_FIX_GW, STA_FIX_SN)) {
    Serial.println("[WIFI] STA Failed to configure Static IP!");
  }

  WiFi.begin(STA_SSID, STA_PASS);
  
  // HTTP Server (Backup)
  server.on("/", [](){ server.send(200, "text/plain", "UWB Tag 1 Online"); });
  server.on("/json", handleJson);
  server.on("/info", handleInfo);
  server.on("/cal", []() { handleCalCommon(false); });
  server.on("/calp", []() { handleCalCommon(true); });
  server.on("/save", handleSave);
  server.on("/reset", handleReset);
  server.begin(); 

  // WebSocket Server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("[WS] WebSocket Server started on port 81");
}

void loopWeb() { 
  server.handleClient(); 
  webSocket.loop(); 

  // ส่งข้อมูล JSON ทุก 100ms
  static uint32_t last_ws_send = 0;
  if (millis() - last_ws_send > 100) {
    last_ws_send = millis();
    if(webSocket.connectedClients() > 0) {
        webSocket.broadcastTXT(buildJson());
    }
  }
}