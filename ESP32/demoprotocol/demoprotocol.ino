#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h> 
#include "Settings.h"

// --- Global Objects ---
HardwareSerial STM32Serial(2);
WebServer server(80);

// 🔥 ตัวแปรเก็บมุมจาก STM32
float currentAngle = 0.0;

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

void enableCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// 🔥 ฟังก์ชันใหม่: ดักฟัง STM32
void checkSTM32() {
  while (STM32Serial.available()) {
    String line = STM32Serial.readStringUntil('\n');
    line.trim(); 
    
    // ถ้าเจอ "A=" ให้เก็บค่ามุม
    if (line.startsWith("A=")) {
      currentAngle = line.substring(2).toFloat();
      Serial.print("Parsed Angle: "); 
      Serial.println(currentAngle);
      // Serial.println("Angle Updated: " + String(currentAngle)); // Debug
    }
  }
}

void sendToSTM32(String cmd) {
  String serialCmd = "";
  if      (cmd == "FWD") serialCmd = "V,300,0";
  else if (cmd == "BWD") serialCmd = "V,-300,0";
  else if (cmd == "LEFT") serialCmd = "V,250,150";
  else if (cmd == "RIGHT") serialCmd = "V,250,-150";
  else if (cmd == "ROTL") serialCmd = "V,0,200";  // Spin Left
  else if (cmd == "ROTR") serialCmd = "V,0,-200"; // Spin Right
  else if (cmd == "STOP") serialCmd = "S";
  
  if(serialCmd != "") {
    STM32Serial.println(serialCmd);
    Serial.println("STM32 << " + serialCmd);
  }
}

void handleCommand() {
  enableCORS();
  if (server.method() == HTTP_OPTIONS) { server.send(200, "text/plain", "OK"); return; }

  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    String cmd = "";
    if (body.indexOf("FWD") >= 0) cmd = "FWD";
    else if (body.indexOf("BWD") >= 0) cmd = "BWD";
    else if (body.indexOf("LEFT") >= 0) cmd = "LEFT";
    else if (body.indexOf("RIGHT") >= 0) cmd = "RIGHT";
    else if (body.indexOf("ROTL") >= 0) cmd = "ROTL";
    else if (body.indexOf("ROTR") >= 0) cmd = "ROTR";
    else if (body.indexOf("STOP") >= 0) cmd = "STOP";

    if (cmd != "") {
      sendToSTM32(cmd);
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

  server.on("/cmd", HTTP_POST, handleCommand);
  server.on("/cmd", HTTP_OPTIONS, handleCommand);

  // 🔥 API ใหม่: ดูค่ามุม (GET /status)
  server.on("/status", HTTP_GET, []() {
    enableCORS();
    String json = "{\"status\":\"ok\", \"angle\":" + String(currentAngle) + "}";
    server.send(200, "application/json", json);
  });

  server.on("/", []() {
    server.send(200, "text/plain", "CyberBot API Online. GET /status to see angle.");
  });

  server.begin();
  Serial.println("API Server Ready!");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  
  // 🔥 เรียกอ่านค่าตลอดเวลา
  checkSTM32();
}