#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <ESPmDNS.h>

#include <ArduinoOTA.h>
#include <ArduinoJson.h> // ⚠️ ต้องลง Library "ArduinoJson" เพิ่มนะครับ (ถ้ายังไม่มี)

#include "Settings.h"
// #include "WebPage.h" <-- ลบทิ้งไปเลย เราไม่ใช้ HTML ในนี้แล้ว

// --- Global Objects ---
HardwareSerial STM32Serial(2);
WebServer server(80);

// --- Helper Functions ---
void initWiFi() {
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
}

// ⚠️ ฟังก์ชันสำคัญ: อนุญาตให้เพื่อนยิง API ข้ามเครื่องได้ (CORS)
void enableCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendToSTM32(String cmd) {
  // Logic เดิม: แปลงคำสั่งเป็น Protocol STM32
  String serialCmd = "";
  if      (cmd == "FWD") serialCmd = "V,300,0";
  else if (cmd == "BWD") serialCmd = "V,-300,0";
  else if (cmd == "LEFT") serialCmd = "V,250,150";
  else if (cmd == "RIGHT") serialCmd = "V,250,-150";
  else if (cmd == "STOP") serialCmd = "S";
  
  if(serialCmd != "") {
    STM32Serial.println(serialCmd);
    Serial.println("STM32 << " + serialCmd);
  }
}

void handleCommand() {
  enableCORS(); // ใส่ Header อนุญาตทุกครั้งที่ตอบกลับ

  if (server.method() == HTTP_OPTIONS) {
    server.send(200, "text/plain", "OK");
    return;
  }

  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    Serial.println("Received JSON: " + body);

    String cmd = "";
    if (body.indexOf("FWD") >= 0) cmd = "FWD";
    else if (body.indexOf("BWD") >= 0) cmd = "BWD";
    else if (body.indexOf("LEFT") >= 0) cmd = "LEFT";
    else if (body.indexOf("RIGHT") >= 0) cmd = "RIGHT";
    else if (body.indexOf("STOP") >= 0) cmd = "STOP";

    if (cmd != "") {
      sendToSTM32(cmd);
      // *** แก้ตรงนี้แล้วครับ ***
      server.send(200, "application/json", "{\"status\":\"ok\", \"cmd\":\"" + cmd + "\"}");
    } else {
      server.send(400, "application/json", "{\"status\":\"error\", \"msg\":\"Unknown Command\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\":\"error\", \"msg\":\"Body Missing\"}");
  }
}

void setup() {
  Serial.begin(115200);
  STM32Serial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  initWiFi();
  
  ArduinoOTA.setHostname("CyberBot-API");
  ArduinoOTA.begin();

  // Route ใหม่: /cmd (รับ POST)
  server.on("/cmd", HTTP_POST, handleCommand);
  
  // รองรับกรณีเพื่อนยิงมาเช็คสิทธิ์ (OPTIONS)
  server.on("/cmd", HTTP_OPTIONS, handleCommand);

  // Route หน้าแรก: บอกว่าฉันคือ API Server
  server.on("/", []() {
    server.send(200, "text/plain", "CyberBot API Server Online. Use POST /cmd to control.");
  });

  server.begin();
  Serial.println("API Server Ready!");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}