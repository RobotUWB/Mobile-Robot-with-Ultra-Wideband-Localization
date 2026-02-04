#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


#include "Settings.h"
#include "WebPage.h"

// --- Global Objects ---
HardwareSerial STM32Serial(2);
WebServer server(80);
unsigned long previousMillis = 0;



void initWiFi() {
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
}

void initOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();
}

void sendToSTM32(String cmd) {
  if(cmd != "") {
    STM32Serial.println(cmd);
    Serial.println("STM32 << " + cmd);
    server.send(200, "text/plain", "STM32: " + cmd);
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleAction() {
  String dir = server.arg("go");
  String cmd = "";

  // Logic การแปลงปุ่มเป็น Command
  if      (dir == "F") cmd = "V,300,0";
  else if (dir == "B") cmd = "V,-300,0";
  else if (dir == "L") cmd = "V,250,150";
  else if (dir == "R") cmd = "V,250,-150";
  else if (dir == "Q") cmd = "V,0,300";
  else if (dir == "E") cmd = "V,0,-300";
  else if (dir == "S") cmd = "S";

  sendToSTM32(cmd);
}

void checkWiFi() {
  unsigned long currentMillis = millis();
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= RECONNECT_INTERVAL)) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
}

// --- Main Setup & Loop ---

void setup() {
  Serial.begin(115200);
  STM32Serial.begin(115200, SERIAL_8N1, TX_PIN, RX_PIN); // ใช้ค่าจาก Settings.h

  initWiFi();
  initOTA();

  // Web Server Routes
  server.on("/", []() { server.send(200, "text/html; charset=utf-8", index_html); });
  server.on("/action", handleAction);
  server.begin();
  
  Serial.println("System Ready!");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  checkWiFi();
}