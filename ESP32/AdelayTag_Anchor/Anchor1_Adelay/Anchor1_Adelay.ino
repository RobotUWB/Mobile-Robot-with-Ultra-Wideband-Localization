#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// MAC Address ของ Anchor 1
char anchor_addr[] = "84:00:00:00:00:00:00:01";

// ==========================================
// 🎯 จุดที่ต้องเปลี่ยนเพื่อจูนให้ได้ 2.00 เมตร 🎯
// ==========================================
uint16_t Adelay = 16620; 

// Pin Configuration
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS    5

const uint8_t PIN_RST = 22;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = 5;

// ตัวแปรสำหรับหาค่าเฉลี่ยให้นิ่งขึ้น (อ่าน 10 ครั้งแล้วค่อยพริ้นต์)
float sum_distance = 0;
int read_count = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ANCHOR 1 CALIBRATION MODE ===");
  
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, DW_CS);
  pinMode(DW_CS, OUTPUT);
  digitalWrite(DW_CS, HIGH);

  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  DW1000.setAntennaDelay(Adelay);

  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.startAsAnchor(anchor_addr, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);
}

void loop() {
  DW1000Ranging.loop();
}

void newRange() {
  float current_distance = DW1000Ranging.getDistantDevice()->getRange();
  
  // กรองค่าที่ผิดปกติออก
  if (isnan(current_distance) || current_distance <= 0) return;

  sum_distance += current_distance;
  read_count++;

  // ครบ 10 ครั้งแล้วค่อยแสดงผล จะได้อ่านทัน
  if (read_count >= 10) {
    float avg_distance = sum_distance / 10.0;
    
    Serial.print("ค่า Adelay ปัจจุบัน: "); 
    Serial.print(Adelay);
    Serial.print(" | ระยะที่อ่านได้: "); 
    Serial.print(avg_distance, 2);
    Serial.print(" m");

    // คำแนะนำการปรับค่า
    if (avg_distance > 2.03) {
      Serial.println("  => ไกลไป! ลอง **เพิ่ม** ค่า Adelay ขึ้นทีละ 10-20");
    } else if (avg_distance < 1.97) {
      Serial.println("  => ใกล้ไป! ลอง **ลด** ค่า Adelay ลงทีละ 10-20");
    } else {
      Serial.println("  => ✅ นิ่งแล้ว! ระยะ 2 เมตรเป๊ะ จดค่า Adelay นี้ไว้ใช้ได้เลย");
    }

    // รีเซ็ตเพื่อคำนวณรอบใหม่
    sum_distance = 0;
    read_count = 0;
  }
}