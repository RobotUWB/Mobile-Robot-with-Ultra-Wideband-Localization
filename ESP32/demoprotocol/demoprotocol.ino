#include <WiFi.h>
#include <HardwareSerial.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include "Settings.h" 

// --- Global Objects ---
HardwareSerial STM32Serial(2);
WebSocketsServer webSocket = WebSocketsServer(81); 

// --- Variables ---
float currentAngle = 0.0;
unsigned long previousWiFiCheck = 0;
const long wifiCheckInterval = 5000; // เช็ค WiFi ทุก 5 วินาที
bool isWiFiConnected = false;

// --- Serial Buffer ---
const int MAX_BUFFER_SIZE = 100;
char inputBuffer[MAX_BUFFER_SIZE];
int bufferIndex = 0;

// --- Non-blocking WiFi Functions ---
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("WiFi: Connecting in background...");
}

void handleWiFiStateMachine() {
  unsigned long currentMillis = millis();
  
  // 1. อ่าน State ปัจจุบันของ Hardware
  bool currentStatus = (WiFi.status() == WL_CONNECTED);

  // 2. Event: เพิ่งต่อติดครั้งแรก หรือกลับมาต่อติดใหม่ (Edge Trigger)
  if (currentStatus && !isWiFiConnected) {
    Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
    isWiFiConnected = true;
  }
  
  // 3. Event: สัญญาณเพิ่งหลุด (Edge Trigger)
  else if (!currentStatus && isWiFiConnected) {
    Serial.println("\nWiFi Lost! System will auto-reconnect in background...");
    isWiFiConnected = false;
  }

  // 4. State: กำลังพยายามเชื่อมต่อ (ทำงานทุกๆ wifiCheckInterval)
  if (!currentStatus) {
    if (currentMillis - previousWiFiCheck >= wifiCheckInterval) {
      previousWiFiCheck = currentMillis;
      Serial.println("Status: Reconnecting to WiFi...");
      WiFi.reconnect(); // สั่งให้ ESP32 พยายามเชื่อมต่อใหม่แบบไม่บล็อกโค้ด
    }
  }
}

void checkWiFiConnection() {
  unsigned long currentMillis = millis();
  // เช็คทุกๆ 5 วินาที (Non-blocking)
  if (currentMillis - previousWiFiCheck >= wifiCheckInterval) {
    previousWiFiCheck = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi Lost! Reconnecting...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
}

// ฟังก์ชันส่งข้อมูลไป STM32 แบบมี Checksum
void sendToSTM32(String cmd) {
  String serialCmd = "";
  if      (cmd == "FWD")   serialCmd = "V,300,0";
  else if (cmd == "BWD")   serialCmd = "V,-300,0";
  else if (cmd == "LEFT")  serialCmd = "V,250,150";
  else if (cmd == "RIGHT") serialCmd = "V,250,-150";
  else if (cmd == "ROTL")  serialCmd = "V,0,200";
  else if (cmd == "ROTR")  serialCmd = "V,0,-200";
  else if (cmd == "STOP")  serialCmd = "S";

  if(serialCmd != "") {
    // 1. คำนวณ XOR Checksum
    uint8_t cs = 0;
    for (int i = 0; i < serialCmd.length(); i++) {
        cs ^= serialCmd[i];
    }
    
    // 2. จัดรูปแบบข้อความให้เป็น cmd*CS\n
    char buf[10];
    sprintf(buf, "*%02X\n", cs);
    String finalCmd = serialCmd + String(buf);
    
    // 3. ส่งไป STM32 (ใช้ print เพราะใส่ \n ไว้แล้ว)
    STM32Serial.print(finalCmd);
    Serial.print("STM32 << " + finalCmd);
    
    // 4. Feedback กลับไปหน้าเว็บ
    String ackJson = "{\"type\":\"ack\", \"cmd\":\"" + cmd + "\", \"status\":\"sent\"}";
    webSocket.broadcastTXT(ackJson);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected\n", num);
      break;
    case WStype_TEXT:
      String cmd = String((char*)payload);
      sendToSTM32(cmd);
      break;
  }
}

// ระบบอ่านข้อมูลที่มี Checksum Filtering
void checkSTM32() {
  while (STM32Serial.available()) {
    char c = STM32Serial.read();
    
    // 1. ถ้าเจอ Newline (\n) แสดงว่าจบบรรทัด
    if (c == '\n') {
      inputBuffer[bufferIndex] = '\0';
      String line = String(inputBuffer);
      line.trim();
      bufferIndex = 0;

      // ตรวจหาเครื่องหมาย * ของ Checksum
      int starIndex = line.lastIndexOf('*');
      
      if (starIndex > 0) { 
        // แยกข้อความกับ Checksum ออกจากกัน
        String payload = line.substring(0, starIndex);
        String recvCsStr = line.substring(starIndex + 1);
        
        // คำนวณ Checksum เทียบกับสิ่งที่รับมา
        uint8_t calcCs = 0;
        for (int i = 0; i < payload.length(); i++) {
          calcCs ^= payload[i];
        }
        
        // แปลง String ฐาน 16 เป็นตัวเลข
        uint8_t recvCs = (uint8_t) strtol(recvCsStr.c_str(), NULL, 16);
        
        // ถ้ารหัสผ่านการตรวจสอบ
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
        // ข้อความไม่มี Checksum (เช่น Debug log ปกติ)
        if (line.length() > 0) {
          Serial.println("STM32 LOG: " + line);
        }
      }
    } 
    // 2. ถ้ายังไม่จบ ให้เก็บใส่ Buffer
    else {
      if (bufferIndex < MAX_BUFFER_SIZE - 1) {
        if (c >= 32) { // กรองขยะ
          inputBuffer[bufferIndex++] = c;
        }
      } else {
        bufferIndex = 0; // กัน Buffer overflow
      }
    }
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
  
  Serial.println("System Ready. Checksum Protocol Active.");
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
  checkSTM32();        
  
  handleWiFiStateMachine();
}