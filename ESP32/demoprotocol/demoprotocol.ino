#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>

// --- 1. ตั้งค่า WiFi บ้าน (GMR) ---
const char* ssid = "GMR";
const char* password = "12123121211212312121";

// --- 2. ตั้งค่า UART (คุยกับ STM32) ---
HardwareSerial STM32Serial(2); // RX=16, TX=17

// สร้าง Web Server ที่พอร์ต 80
WebServer server(80);

// --- 3. หน้าเว็บ HTML (UI บังคับหุ่นยนต์) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>ROS Robot Controller</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@500;700&display=swap');
    :root { --primary: #00f3ff; --danger: #ff003c; --bg: #0b0c10; --panel: #1f2833; --btn-shadow: #000; }
    body { font-family: 'Orbitron', sans-serif; background-color: var(--bg); color: var(--primary); text-align: center; margin: 0; padding: 0; height: 100vh; display: flex; flex-direction: column; justify-content: center; align-items: center; overflow: hidden; user-select: none; }
    h1 { margin-bottom: 5px; font-size: 22px; text-shadow: 0 0 10px var(--primary); letter-spacing: 2px; }
    p { color: #c5c6c7; font-size: 12px; letter-spacing: 1px; margin-bottom: 25px; opacity: 0.8; }
    .container { position: relative; width: auto; background: rgba(31, 40, 51, 0.4); border-radius: 20px; padding: 30px; box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.7); backdrop-filter: blur(8px); border: 1px solid rgba(0, 243, 255, 0.1); }
    .btn { width: 80px; height: 80px; background: linear-gradient(145deg, #232d3a, #1d2530); border-radius: 12px; border: none; color: var(--primary); font-size: 32px; cursor: pointer; box-shadow: 5px 5px 10px #0b0e11, -5px -5px 10px #2f3c4b; transition: all 0.1s ease; -webkit-tap-highlight-color: transparent; outline: none; margin: 5px; display: inline-flex; justify-content: center; align-items: center; }
    .btn:active { transform: translateY(2px); box-shadow: inset 5px 5px 10px #0b0e11, inset -5px -5px 10px #2f3c4b; color: #fff; text-shadow: 0 0 15px var(--primary); }
    .btn-stop { width: 80px; height: 80px; font-size: 14px; font-weight: bold; color: var(--danger); border: 1px solid rgba(255, 0, 60, 0.3); }
    .btn-stop:active { background: var(--danger); color: white; box-shadow: 0 0 20px var(--danger); }
    .row { display: flex; justify-content: center; align-items: center; }
  </style>
</head>
<body>
  <h1>&#129302; CYBER BOT</h1>
  <p>SYSTEM ONLINE</p>
  
  <div class="container">
    <div class="row">
      <button class="btn" ontouchstart="move('F')" onmousedown="move('F')">&#9650;</button>
    </div>
    <div class="row">
      <button class="btn" ontouchstart="move('L')" onmousedown="move('L')">&#9664;</button>
      <button class="btn btn-stop" ontouchstart="move('S')" onmousedown="move('S')">STOP</button>
      <button class="btn" ontouchstart="move('R')" onmousedown="move('R')">&#9654;</button>
    </div>
    <div class="row">
      <button class="btn" ontouchstart="move('B')" onmousedown="move('B')">&#9660;</button>
    </div>
  </div>

  <script>
    function move(dir) {
      if(navigator.vibrate) navigator.vibrate(30);
      var x = new XMLHttpRequest();
      x.open("GET", "/action?go=" + dir, true);
      x.send();
    }
    document.addEventListener('contextmenu', event => event.preventDefault());
  </script>
</body>
</html>
)rawliteral";

void setup() {
  // เริ่มต้น Serial
  Serial.begin(115200);
  STM32Serial.begin(115200, SERIAL_8N1, 16, 17); // Check wiring: ESP TX(17)->STM RX, ESP RX(16)->STM TX

  // --- เชื่อมต่อ WiFi ---
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); 

  // --- ตั้งค่า Server Handlers ---

  // 1. หน้าแรก
  server.on("/", []() {
    server.send(200, "text/html; charset=utf-8", index_html);
  });

  // 2. รับคำสั่ง (/action?go=X) และแปลงเป็น ROS Protocol
  server.on("/action", []() {
    String dir = server.arg("go");
    Serial.println("User Input: " + dir); 

    // --- 🔥 แปลงคำสั่งเป็น ROS Protocol (V,linear,angular) ---
    // รูปแบบ: V,ความเร็วเดินหน้า,ความเร็วเลี้ยว
    
    if (dir == "F") {
        // เดินหน้า: วิ่ง 300 mm/s, ไม่เลี้ยว
        STM32Serial.println("V,300,0"); 
    }
    else if (dir == "B") {
        // ถอยหลัง: วิ่ง -300 mm/s, ไม่เลี้ยว
        STM32Serial.println("V,-300,0"); 
    }
    else if (dir == "L") {
        // เลี้ยวซ้ายแบบ Diff-Drive (เดินหน้า 250 + เอียงซ้าย 150)
        // ผลลัพธ์: ล้อซ้ายจะช้า (100), ล้อขวาจะเร็ว (400) -> โค้งซ้ายสวยๆ
        STM32Serial.println("V,250,150"); 
    }
    else if (dir == "R") {
        // เลี้ยวขวาแบบ Diff-Drive (เดินหน้า 250 + เอียงขวา 150)
        // ผลลัพธ์: ล้อซ้ายเร็ว (400), ล้อขวาช้า (100) -> โค้งขวาสวยๆ
        STM32Serial.println("V,250,-150"); 
    }
    else if (dir == "S") {
        // หยุดฉุกเฉิน
        STM32Serial.println("S"); 
    }

    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("Web Server Started!");
}

void loop() {
  server.handleClient();
}