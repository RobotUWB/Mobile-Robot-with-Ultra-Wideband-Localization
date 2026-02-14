#include <WiFi.h>
#include <HardwareSerial.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include "Settings.h" 

// --- Global Objects ---
HardwareSerial STM32Serial(2);
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT); 

// --- Variables ---
float currentAngle = 0.0;
unsigned long previousWiFiCheck = 0;
bool isWiFiConnected = false;

// --- Serial Buffer ---
char inputBuffer[MAX_SERIAL_BUFFER];
int bufferIndex = 0;

// --- Heartbeat Timers ---
unsigned long lastWebHeartbeat = 0;
unsigned long lastSTM32Heartbeat = 0;

// --- Non-blocking WiFi Functions ---
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("WiFi: Connecting in background...");
}

void handleWiFiStateMachine() {
  unsigned long currentMillis = millis();
  
  bool currentStatus = (WiFi.status() == WL_CONNECTED);

  if (currentStatus && !isWiFiConnected) {
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
    isWiFiConnected = true;
  }
  else if (!currentStatus && isWiFiConnected) {
    Serial.println("\nWiFi Lost! System will auto-reconnect in background...");
    isWiFiConnected = false;
  }

  if (!currentStatus) {
    if (currentMillis - previousWiFiCheck >= WIFI_CHECK_INTERVAL) {
      previousWiFiCheck = currentMillis;
      Serial.println("Status: Reconnecting to WiFi...");
      WiFi.reconnect();
    }
  }
}

// --- STM32 Communication ---
void sendToSTM32(String cmd) {
  String serialCmd = "";
  if      (cmd == "FWD")   serialCmd = "V,300,0";
  else if (cmd == "BWD")   serialCmd = "V,-300,0";
  else if (cmd == "LEFT")  serialCmd = "V,250,150";
  else if (cmd == "RIGHT") serialCmd = "V,250,-150";
  else if (cmd == "ROTL")  serialCmd = "V,0,200";
  else if (cmd == "ROTR")  serialCmd = "V,0,-200";
  else if (cmd == "STOP")  serialCmd = "S";
<<<<<<< HEAD
  
  if(serialCmd != "") {
    // 1. ส่งไป STM32
    for (int i = 0; i < serialCmd.length(); i++) {
        cs ^= serialCmd[i];
    }
    
    char buf[10];
    sprintf(buf, "*%02X\n", cs);
    String finalCmd = serialCmd + String(buf);
    
    STM32Serial.print(finalCmd);
    
    if (cmd != "H") {
      Serial.print("STM32 << " + finalCmd);
      String ackJson = "{\"type\":\"ack\", \"cmd\":\"" + cmd + "\", \"status\":\"sent\"}";
      webSocket.broadcastTXT(ackJson);
    }
  }
}

void checkSTM32() {
  while (STM32Serial.available()) {
    char c = STM32Serial.read();
    
    if (c == '\n') {
      inputBuffer[bufferIndex] = '\0';
      String line = String(inputBuffer);
      line.trim();
      bufferIndex = 0;

      int starIndex = line.lastIndexOf('*');
      
      if (starIndex > 0) { 
        String payload = line.substring(0, starIndex);
        String recvCsStr = line.substring(starIndex + 1);
        
        uint8_t calcCs = 0;
        for (int i = 0; i < payload.length(); i++) {
          calcCs ^= payload[i];
        }
        
        uint8_t recvCs = (uint8_t) strtol(recvCsStr.c_str(), NULL, 16);
        
        if (calcCs == recvCs) {
          if (payload.startsWith("A=")) {
            currentAngle = payload.substring(2).toFloat();
            String json = "{\"type\":\"data\", \"angle\":" + String(currentAngle) + "}";
            webSocket.broadcastTXT(json);
          }
          else if (payload == "OK") { 
            webSocket.broadcastTXT("{\"type\":\"ack\", \"status\":\"stm_confirmed\"}");
          }
        } else {
          Serial.println("Warning: Checksum mismatch from STM32. Dropped.");
        }
      } else {
        if (line.length() > 0) {
          Serial.println("STM32 LOG: " + line);
        }
      }
    } 
    else {
      if (bufferIndex < MAX_SERIAL_BUFFER - 1) {
        if (c >= 32) { 
          inputBuffer[bufferIndex++] = c;
        }
      } else {
        bufferIndex = 0; 
      }
    }
  }
}

// --- WebSocket & Heartbeat ---
>>>>>>> parent of 8ab0966 (Revert "Merge branch 'udev' of https://github.com/RobotUWB/Mobile-Robot-with-Ultra-Wideband-Localization into Ice")
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      break;
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected\n", num);
      lastWebHeartbeat = millis(); 
      break;
    case WStype_TEXT: {
      String payloadStr = String((char*)payload);
      if (payloadStr == "PING") {
        lastWebHeartbeat = millis();
        webSocket.broadcastTXT("PONG");
      } else {
        sendToSTM32(payloadStr);
      }
      break;
    }
  }
}

void handleHeartbeat() {
  unsigned long currentMillis = millis();

  if (webSocket.connectedClients() > 0) {
    if (currentMillis - lastWebHeartbeat > WEB_TIMEOUT_MS) {
      Serial.println("Web Timeout! Emergency Brake Activated.");
      sendToSTM32("STOP"); 
      webSocket.disconnect(); 
      lastWebHeartbeat = currentMillis; 
    }
  }

  if (currentMillis - lastSTM32Heartbeat >= STM32_HEARTBEAT_INTERVAL) {
    lastSTM32Heartbeat = currentMillis;
    sendToSTM32("H");
  }
}

void setup() {
  Serial.begin(115200);
  STM32Serial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  initWiFi();
  
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("System Ready. Full Protocol Active.");
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
  checkSTM32();        
  handleWiFiStateMachine();
  handleHeartbeat(); 
}