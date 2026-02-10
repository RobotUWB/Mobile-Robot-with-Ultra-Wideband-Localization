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

// --- Serial Buffer ---
const int MAX_BUFFER_SIZE = 100;
char inputBuffer[MAX_BUFFER_SIZE];
int bufferIndex = 0;

// --- Helper Functions ---
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  // รอแค่ 10 วินาทีพอ ถ้าไม่ได้ให้ข้ามไปทำงานต่อ (เดี๋ยว Reconnect loop จัดการเอง)
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) { 
    delay(500); 
    Serial.print("."); 
    retry++;
  }
  Serial.println("\nWiFi Init Complete (Status: " + String(WiFi.status()) + ")");
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

// ฟังก์ชันส่งข้อมูลไป STM32 และแจ้งกลับหน้าเว็บ (Feedback)
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
    // 1. ส่งไป STM32
    STM32Serial.println(serialCmd);
    Serial.println("STM32 << " + serialCmd);

    // 2. Feedback กลับไปหน้าเว็บ (ACK)
    // บอก Client ว่า "ได้รับคำสั่งแล้วนะ ส่งไปให้หุ่นยนต์แล้ว"
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

// Robust Serial Reading (State Machine / Buffering)
void checkSTM32() {
  while (STM32Serial.available()) {
    char c = STM32Serial.read();

    // 1. ถ้าเจอ Newline (\n) แสดงว่าจบบรรทัด -> ประมวลผลทันที
    if (c == '\n') {
      inputBuffer[bufferIndex] = '\0'; // ปิดท้าย String
      String line = String(inputBuffer);
      line.trim(); // ตัด \r หรือช่องว่างส่วนเกิน
      
      // Reset Buffer สำหรับรอบถัดไป
      bufferIndex = 0;

      // ประมวลผลข้อมูล
      if (line.startsWith("A=")) {
        currentAngle = line.substring(2).toFloat();
        // ส่ง JSON ระบุ type เพื่อให้หน้าเว็บแยกแยะได้ง่ายขึ้น
        String json = "{\"type\":\"data\", \"angle\":" + String(currentAngle) + "}";
        webSocket.broadcastTXT(json);
      }
      else if (line == "OK") { 
        // กรณี STM32 ตอบ OK กลับมา (ถ้าคุณเขียนเพิ่มใน STM32)
        webSocket.broadcastTXT("{\"type\":\"ack\", \"status\":\"stm_confirmed\"}");
      }
    } 
    // 2. ถ้ายังไม่จบ ให้เก็บใส่ Buffer
    else {
      if (bufferIndex < MAX_BUFFER_SIZE - 1) {
        // กรองตัวอักษรขยะ หรือ \r ออก
        if (c >= 32) { 
          inputBuffer[bufferIndex++] = c;
        }
      } else {
        // Buffer เต็ม (Overflow protection) -> Reset ทิ้งเพื่อกัน error
        bufferIndex = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  STM32Serial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  initWiFi();
  
  ArduinoOTA.setHostname("CyberBot-Robust");
  ArduinoOTA.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("System Ready.");
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
  checkSTM32();        
  checkWiFiConnection(); // เพิ่มบรรทัดนี้เพื่อเช็คเน็ตตลอดเวลา
}