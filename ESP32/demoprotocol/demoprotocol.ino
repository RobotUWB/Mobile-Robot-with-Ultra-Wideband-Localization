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
// ออกแบบให้ปุ่มใหญ่ๆ กดง่ายๆ บนมือถือ
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Robot Control</title>
  <style>
    body { font-family: Arial; text-align: center; background-color: #2c3e50; color: white; }
    h1 { margin-top: 20px; }
    .btn { background-color: #3498db; border: none; color: white; padding: 20px 0;
           width: 120px; font-size: 24px; margin: 10px; cursor: pointer; 
           border-radius: 15px; box-shadow: 0 5px #2980b9; user-select: none; }
    .btn:active { background-color: #2980b9; box-shadow: 0 2px #2980b9; transform: translateY(3px); }
    .stop { background-color: #e74c3c; box-shadow: 0 5px #c0392b; width: 260px; }
    .stop:active { background-color: #c0392b; box-shadow: 0 2px #c0392b; }
    .container { display: flex; flex-direction: column; align-items: center; justify-content: center; height: 80vh; }
    .row { display: flex; justify-content: center; }
  </style>
</head>
<body>
  <h1> ;-; </h1>
  
  <div class="container">
    <div class="row">
      <button class="btn" onclick="move('F')">&#8593;</button>
    </div>

    <div class="row">
      <button class="btn" onclick="move('L')">&#8592;</button>
      <button class="btn" onclick="move('R')">&#8594;</button>
    </div>
    
    <div class="row">
      <button class="btn stop" onclick="move('S')">STOP</button>
    </div>

    <div class="row">
      <button class="btn" onclick="move('B')">&#8595;</button>
    </div>
  </div>

  <script>
    // ฟังก์ชันส่งคำสั่งไปที่ ESP32 โดยไม่ต้องโหลดหน้าเว็บใหม่ (AJAX)
    function move(dir) {
      var xhttp = new XMLHttpRequest();
      xhttp.open("GET", "/action?go=" + dir, true);
      xhttp.send();
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
  // เริ่มต้น Serial
  Serial.begin(115200);
  STM32Serial.begin(115200, SERIAL_8N1, 16, 17); // RX=16, TX=17

  // --- เชื่อมต่อ WiFi ---
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA); // โหมด Station (เกาะ WiFi GMR)
  WiFi.begin(ssid, password);

  // รอจนกว่าจะต่อติด
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // *** จำเลขนี้ไว้พิมพ์ใน Browser ***

  // --- ตั้งค่า Server ---
  
  // 1. หน้าแรก (Root) -> แสดงปุ่มกด
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  // 2. รับคำสั่งกดปุ่ม (/action?go=F)
  server.on("/action", []() {
    String dir = server.arg("go");
    Serial.println("Command: " + dir); // Debug ในคอม

    // แปลงเป็น Protocol ส่งให้ STM32
    // println จะเติม \r\n (เหมือน #010) ให้เองอัตโนมัติ
    if (dir == "F") STM32Serial.println("M,1000");   // เดินหน้า 1 เมตร
    else if (dir == "B") STM32Serial.println("M,-1000"); // ถอยหลัง 1 เมตร
    else if (dir == "L") STM32Serial.println("T,-90");  // เลี้ยวซ้าย 90 องศา
    else if (dir == "R") STM32Serial.println("T,90");   // เลี้ยวขวา 90 องศา
    else if (dir == "S") STM32Serial.println("S");      // หยุดฉุกเฉิน

    server.send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("Web Server Started!");
}

void loop() {
  server.handleClient(); // รอรับ request จากมือถือ
}