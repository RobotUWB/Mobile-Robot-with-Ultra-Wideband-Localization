#include "WebWorker.h"
#include "Shared.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h> 

// [Brownout Fix]
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

WebSocketsServer webSocket = WebSocketsServer(81);
WebServer server(80);
char msgBuffer[1024]; 

// =================== ตัวแปร Flag สำหรับฝากคำสั่ง ===================
volatile bool req_reset = false; // ตัวแปรฝากคำสั่ง Reset
volatile bool req_save  = false; // ตัวแปรฝากคำสั่ง Save

// ... (ส่วน WIFI CONFIG คงเดิม) ...
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

// ... (ฟังก์ชัน buildJson และ calStart คงเดิม) ...
char* buildJson(int mode) {
  // ... (โค้ดเดิมของคุณ) ...
  // ตัดมาเฉพาะส่วนสำคัญเพื่อความกระชับ
  bool t2_online = (millis() - t2_last_ms < 3000); 
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

  // ... (Logic การสร้าง JSON เหมือนเดิม) ...
  if (mode == 0) {
      snprintf(msgBuffer, sizeof(msgBuffer), 
        "{\"x\":%.3f,\"y\":%.3f,\"rmse\":%.4f,\"n\":%d,\"ok\":%d,\"t2\":{\"ok\":%d,\"x\":%.2f,\"y\":%.2f},\"cs\":%d,\"a\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"field\":{\"w\":%.2f,\"h\":%.2f},\"anch_xy\":[[0.0,0.0],[0.0,2.0],[3.0,0.0],[3.0,2.0]]}",
        safe_x, safe_y, safe_rmse, safe_n, safe_ok ? 1 : 0, t2_online ? 1 : 0, t2_x, t2_y, safe_state,
        anchorString(0).c_str(), anchorString(1).c_str(), anchorString(2).c_str(), anchorString(3).c_str(), FIELD_W, FIELD_H
      );
  } else {
      snprintf(msgBuffer, sizeof(msgBuffer), 
        "{\"x\":%.3f,\"y\":%.3f,\"rmse\":%.4f,\"n\":%d,\"ok\":%d,\"t2\":{\"ok\":%d,\"x\":%.2f,\"y\":%.2f},\"cs\":%d,\"a\":[\"%s\",\"%s\",\"%s\",\"%s\"]}",
        safe_x, safe_y, safe_rmse, safe_n, safe_ok ? 1 : 0, t2_online ? 1 : 0, t2_x, t2_y, safe_state,
        anchorString(0).c_str(), anchorString(1).c_str(), anchorString(2).c_str(), anchorString(3).c_str()
      );
  }
  return msgBuffer;
}

static void calStart(bool multi, float x, float y) {
  cal_ref_x = x; cal_ref_y = y; cal_active = true; cal_multi_point = multi;
  cal_start_ms = millis();
  cal_state = 1; 
  for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
  Serial.printf("[WS] Cal Start: x=%.2f y=%.2f multi=%d\n", x, y, multi);
}

// ... (HTTP Handlers คงเดิม) ...
static void handleCalCommon(bool multi) {
  if (!server.hasArg("x")) return;
  float valX = server.arg("x").toFloat();
  float valY = server.arg("y").toFloat();
  calStart(multi, valX, valY);
  server.send(200, "text/plain", "OK");
}

static void handleSave() { req_save = true; server.send(200, "text/plain", "Saved Pending"); }
static void handleReset() { req_reset = true; server.send(200, "text/plain", "Reset Pending"); }
static void handleInfo() { server.send(200, "text/plain", "UWB System Online"); }
static void handleJson() { server.send(200, "application/json", buildJson(0)); }

IPAddress activeIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP();
  return WiFi.softAPIP();
}

// =================== แก้ไขตรงนี้: ลดภาระใน Callback ===================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED: break;
    case WStype_CONNECTED: webSocket.sendTXT(num, buildJson(0)); break;
    case WStype_TEXT:
      {
        char cmdBuffer[128];
        if (length >= sizeof(cmdBuffer)) length = sizeof(cmdBuffer) - 1;
        memcpy(cmdBuffer, payload, length);
        cmdBuffer[length] = '\0';
        
        String text = String(cmdBuffer);
        text.trim(); 
        
        if (text.startsWith("CAL ")) {
           float valX = 0, valY = 0;
           if (sscanf(text.c_str(), "CAL %f %f", &valX, &valY) == 2) calStart(false, valX, valY);
        }
        else if (text.startsWith("CALP")) {
           float valX = 0, valY = 0;
           if (sscanf(text.c_str(), "CALP %f %f", &valX, &valY) == 2) calStart(true, valX, valY);
        }
        else if (text == "SAVE") {
           req_save = true; // [แก้ไข] แค่ยกธงบอกว่า "อยากเซฟนะ" แล้วจบ
           Serial.println("[WS] Save Requested");
        }
        else if (text == "RESET") {
           req_reset = true; // [แก้ไข] แค่ยกธงบอกว่า "อยากรีเซตนะ"
           Serial.println("[WS] Reset Requested");
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
  if (!WiFi.config(STA_FIX_IP, STA_FIX_GW, STA_FIX_SN)) Serial.println("[WIFI] STA IP Fail");
  WiFi.begin(STA_SSID, STA_PASS);
  
  server.on("/", [](){ server.send(200, "text/plain", "UWB Tag 1 Online"); });
  server.on("/json", handleJson);
  server.on("/info", handleInfo);
  server.on("/cal", []() { handleCalCommon(false); });
  server.on("/calp", []() { handleCalCommon(true); });
  server.on("/save", handleSave);
  server.on("/reset", handleReset);
  server.begin(); 
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("[WS] Server Started");
}

// =================== แก้ไขตรงนี้: ทำงานหนักใน Loop แทน ===================
void performResetSafe() {
   portENTER_CRITICAL(&myMutex); // ล็อคเพื่อความปลอดภัยสูงสุด
   for (int i = 0; i < 4; i++) { 
       bias[i] = 0; 
       mp_bias_sum[i] = 0; 
       mp_bias_cnt[i] = 0; 
       fA[i] = -1; 
       lastAccepted[i] = -1; 
   }
   cal_state = 4;
   portEXIT_CRITICAL(&myMutex);
   
   saveBias(); // เขียน Flash ตอนนี้ปลอดภัยกว่า
   Serial.println("[MAIN] Reset & Save Done.");
}

void performSaveSafe() {
   for (int i = 0; i < 4; i++) if (mp_bias_cnt[i] > 0) bias[i] = mp_bias_sum[i] / mp_bias_cnt[i];
   saveBias();
   Serial.println("[MAIN] Bias Saved.");
}

void loopWeb() { 
  server.handleClient(); 
  webSocket.loop(); 

  // [ส่วนที่เพิ่ม] ตรวจสอบธงคำสั่งแล้วทำงาน
  if (req_reset) {
    req_reset = false;
    performResetSafe();
  }
  
  if (req_save) {
    req_save = false;
    performSaveSafe();
  }

  static uint32_t last_ws_send = 0;
  if (millis() - last_ws_send > 100) {
    last_ws_send = millis();
    if(webSocket.connectedClients() > 0) {
        webSocket.broadcastTXT(buildJson(1));
    }
  }
}