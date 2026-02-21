#include <WiFi.h>
#include <HardwareSerial.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include "Settings.h" 

// --- Global Objects ---
HardwareSerial STM32Serial(2);
HardwareSerial UWBSerial(1);
WebSocketsServer webSocket = WebSocketsServer(WEBSOCKET_PORT); 



// --- Variables ---
float currentAngle = 0.0;
float angleOffset = 0.0; // Heading calibration offset
unsigned long previousWiFiCheck = 0;
bool isWiFiConnected = false;

float currentX = 0.0;
float currentY = 0.0;

// --- Navigation Variables ---
float targetX = 0.0;
float targetY = 0.0;
bool isNavigating = false;
unsigned long lastNavUpdate = 0;
const int NAV_UPDATE_INTERVAL = 100; // 10Hz
const float DIST_THRESHOLD = 0.20;   // 20cm accepted error


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
  else if (cmd == "H")     serialCmd = "H"; 

  if(serialCmd != "") {
    uint8_t cs = 0;
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

void sendToSTM32SerialOnly(String serialCmd) {
  if(serialCmd != "") {
    // Serial.println("Sending to STM32: " + serialCmd); // Debug
    uint8_t cs = 0;
    for (int i = 0; i < serialCmd.length(); i++) cs ^= serialCmd[i];
    char buf[10];
    sprintf(buf, "*%02X\n", cs);
    STM32Serial.print(serialCmd + String(buf));
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
            float rawAngle = payload.substring(2).toFloat();
            // Normalize raw to 0-360 first
            while (rawAngle >= 360) rawAngle -= 360;
            while (rawAngle < 0)    rawAngle += 360;
            
            // Apply Calibration Offset
            currentAngle = rawAngle - angleOffset;
            
            // Re-normalize result
            while (currentAngle >= 360) currentAngle -= 360;
            while (currentAngle < 0)    currentAngle += 360;

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

// --- checkUWB ---
void checkUWB() {
  // เช็กว่ามีข้อมูลส่งมาจาก UWB ไหม
  if (UWBSerial.available()) {
    // อ่านข้อความจนกว่าจะเจอ \n
    String line = UWBSerial.readStringUntil('\n');
    line.trim(); // ตัดพวกช่องว่าง หรือ \r ทิ้งไปให้หมด
    
    Serial.println("RAW UWB: " + line);

    // เช็ก Header ว่าขึ้นต้นด้วย "U," หรือเปล่า
    if (line.startsWith("U,")) {
      
      // หาตำแหน่งของเครื่องหมายลูกน้ำ
      int firstComma = line.indexOf(',');
      int secondComma = line.indexOf(',', firstComma + 1);
      
      // ถ้าเจอคอมม่าครบทั้ง 2 ตัว แสดงว่าฟอร์แมตถูกต้อง
      if (firstComma > 0 && secondComma > 0) {
        // หั่นข้อความเอาเฉพาะตัวเลข
        String xStr = line.substring(firstComma + 1, secondComma);
        String yStr = line.substring(secondComma + 1);
        
        // แปลงเป็นตัวเลขทศนิยม
        currentX = xStr.toFloat() /1000.0;
        currentY = yStr.toFloat() /1000.0;
        
        // ปริ้นท์ออก Serial Monitor เพื่อเช็กความถูกต้อง
        Serial.println("UWB Position -> X: " + String(currentX) + " mm | Y: " + String(currentY) + " mm");
        
        // ส่งข้อมูลขึ้นหน้าเว็บให้เพื่อน Web Dev เอาไปใช้ต่อ
        String json = "{\"type\":\"uwb\", \"x\":" + String(currentX) + ", \"y\":" + String(currentY) + "}";
        webSocket.broadcastTXT(json);
      }
    }
  }
}

// --- WebSocket & Heartbeat ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected\n", num);
      lastWebHeartbeat = millis(); 
      break;
    case WStype_TEXT: {
      String payloadStr = "";
      for (size_t i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
      }
      payloadStr.trim(); 

      // หลังจากนั้นให้ใช้ลอจิกเดิมต่อไป
      if (payloadStr == "PING") {
        lastWebHeartbeat = millis();
        webSocket.broadcastTXT("PONG");
      }
      else if (payloadStr.startsWith("UWB")) { 
        // Optional: Manual Bridge for UWB commands
        UWBSerial.println(payloadStr);
      }
      else if (payloadStr.startsWith("GOTO:")) { // Format: GOTO:x:y
        Serial.println("RX GOTO: " + payloadStr); // Debug Crash
        int firstColon = payloadStr.indexOf(':');
        int secondColon = payloadStr.indexOf(':', firstColon + 1);
        if (secondColon > 0) {
           String xStr = payloadStr.substring(firstColon+1, secondColon);
           String yStr = payloadStr.substring(secondColon+1);
           targetX = xStr.toFloat();
           targetY = yStr.toFloat();
           isNavigating = true;
           Serial.println("Nav Set -> X:" + String(targetX) + " Y:" + String(targetY));
        } else {
           Serial.println("GOTO Format Error");
        }
      }
      else if (payloadStr == "STOP") {
        isNavigating = false;
        sendToSTM32("STOP");
      }
      else if (payloadStr == "SET_ZERO") {
         // Auto-calibrate: Set current raw angle as new Zero
         angleOffset += currentAngle;
         
         // Normalize offset
         while (angleOffset >= 360) angleOffset -= 360;
         while (angleOffset < 0)    angleOffset += 360;
         
         // Force current to 0 immediately for responsiveness
         currentAngle = 0.0;
         
         // อัปเดตค่ามุมให้หน้าเว็บกลับเป็น 0 ทันที
         webSocket.broadcastTXT("{\"type\":\"data\", \"angle\":0.0}");
         
         // --- เพิ่มบรรทัดนี้ ---
         // ส่ง ACK ยืนยันกลับไปให้ฝั่ง Web Dev เอาไปใช้งานต่อ
         webSocket.broadcastTXT("{\"type\":\"ack\", \"cmd\":\"SET_ZERO\", \"status\":\"success\"}");
         
         Serial.println("Heading Calibrated. Zero Set.");
      }
      else if (payloadStr == "H") {
         // Heartbeat: forward to STM32 but DON'T stop navigation
         sendToSTM32("H");
      }
      else { 
        // Manual Drive overrides Auto Nav
        isNavigating = false; 
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
  STM32Serial.begin(115200, SERIAL_8N1, RX_Control, TX_Control);

  UWBSerial.begin(UWB_BAUDRATE, SERIAL_8N1, RX_UWB, TX_UWB);

  initWiFi();
  
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("System Ready. Full Protocol Active.");
}


// --- Navigation Logic ---
void runNavigation() {
  if (!isNavigating) return;

  unsigned long currentMillis = millis();
  if (currentMillis - lastNavUpdate < NAV_UPDATE_INTERVAL) return;
  lastNavUpdate = currentMillis;

  // 1. Calculate Error
  float dx = targetX - currentX;
  float dy = targetY - currentY;
  float distance = sqrt(dx*dx + dy*dy);

  // Check if reached
  if (distance < DIST_THRESHOLD) {
    Serial.println("Nav: Target Reached!");
    sendToSTM32("STOP");
    isNavigating = false;
    webSocket.broadcastTXT("{\"type\":\"ack\", \"status\":\"target_reached\"}");
    return;
  }

  // 2. Calculate Heading Error
  // currentAngle: 0° = -X (หลัง SET_ZERO ตอนหัน -X), CCW+
  // atan2:        0° = +X, CCW+
  // แปลง: worldAngle = currentAngle + 180°
  float worldAngle = currentAngle + 180.0;
  while (worldAngle >= 360) worldAngle -= 360;
  while (worldAngle < 0)    worldAngle += 360;

  float desiredAngle = atan2(dy, dx) * 180.0 / PI;
  // Normalize desiredAngle to 0-360 (atan2 returns -180 to 180)
  while (desiredAngle < 0)    desiredAngle += 360;
  while (desiredAngle >= 360) desiredAngle -= 360;

  float angleError = desiredAngle - worldAngle;
  
  // Normalize angle error (-180 to 180)
  while (angleError > 180) angleError -= 360;
  while (angleError < -180) angleError += 360;

  // 3. Control Logic (Simple Proportional)
  float kp_v = 300.0; // Speed gain
  float kp_w = 2.0;    // Turn Gain (positive w = CCW = left)
  
  float cmd_v = 0;
  float cmd_w = 0;

  if (abs(angleError) > 10.0) {
    // If facing wrong way, turn in place first
    cmd_v = 0;
    cmd_w = angleError * kp_w;
  } else {
    // If facing roughly correct, drive and correct
    cmd_v = distance * kp_v;
    
    // --- Deadzone Fix: Minimum Speed (Safe Mode) ---
    if (cmd_v < 150) cmd_v = 150; // Force min speed to overcome friction
    if (cmd_v > 300) cmd_v = 300; // Cap max speed to prevent Brownout
    
    cmd_w = angleError * kp_w;
  }

  // --- Safety: Clamp Angular Speed ---
  if (cmd_w > 150) cmd_w = 150;
  if (cmd_w < -150) cmd_w = -150;

  // Debug - Check values
  Serial.printf("Nav: Pos=(%.3f,%.3f) Tgt=(%.3f,%.3f) Dist=%.2f\n", 
                currentX, currentY, targetX, targetY, distance);
  Serial.printf("     DesAng=%.1f WorldAng=%.1f CalAng=%.1f AngErr=%.1f | V=%.0f W=%.0f\n", 
                desiredAngle, worldAngle, currentAngle, angleError, cmd_v, cmd_w);
  
  // Optional: Send Debug to Web for easier viewing
  String debugJson = "{\"type\":\"nav_debug\", \"dist\":" + String(distance) + 
                     ", \"err\":" + String(angleError) + 
                     ", \"v\":" + String(cmd_v) + 
                     ", \"w\":" + String(cmd_w) + "}";
  webSocket.broadcastTXT(debugJson);

  // 4. Send to STM32
  // Command format: V,linear_velocity,angular_velocity
  String cmd = "V," + String(cmd_v, 0) + "," + String(cmd_w, 0);
  sendToSTM32SerialOnly(cmd); // New function to avoid broadcast loop
}


void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
  checkSTM32();     
  checkUWB();  
  handleWiFiStateMachine();
  handleHeartbeat(); 
  runNavigation(); // <--- Run Nav Loop
}