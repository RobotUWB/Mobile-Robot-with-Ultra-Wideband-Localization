#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// MAC Address ของ Anchor 2
char anchor_addr[] = "85:00:00:00:00:00:00:02";

// ==========================================
// เริ่มต้นด้วยค่า 16650 ที่ได้จาก Anchor 1
// ==========================================
uint16_t Adelay = 16570; 

// Pin Configuration
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS    5

const uint8_t PIN_RST = 22;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = 5;

// ตัวแปรสำหรับหาค่าเฉลี่ย
float sum_distance = 0;
int read_count = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ANCHOR 2 CALIBRATION MODE ===");
  
  // (แก้แล้ว) ไม่ส่ง DW_CS เข้าไปในคำสั่งนี้ เพื่อไม่ให้บอร์ดค้าง
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
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
  
  if (isnan(current_distance) || current_distance <= 0) return;

  sum_distance += current_distance;
  read_count++;

  // ครบ 10 ครั้งแล้วค่อยแสดงผล
  if (read_count >= 10) {
    float avg_distance = sum_distance / 10.0;
    
    Serial.print("Adelay ปัจจุบัน: "); 
    Serial.print(Adelay);
    Serial.print(" | ระยะที่อ่านได้: "); 
    Serial.print(avg_distance, 2);
    Serial.print(" m");

    // คำแนะนำปรับให้ตรงกับระยะจริง 3.15 เมตรแล้ว
    if (avg_distance > 3.18) {
      Serial.println("  => ไกลไป! ลอง **เพิ่ม** ค่า Adelay ขึ้น");
    } else if (avg_distance < 3.12) {
      Serial.println("  => ใกล้ไป! ลอง **ลด** ค่า Adelay ลง");
    } else {
      Serial.println("  => ✅ นิ่งแล้ว! ระยะประมาณ 3.15 เมตรเป๊ะ จดค่าไว้ได้เลย");
    }

    sum_distance = 0;
    read_count = 0;
  }
}