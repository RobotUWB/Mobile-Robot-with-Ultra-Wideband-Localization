#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// ไลบรารีสำหรับป้องกันบอร์ดรีเซ็ตจากไฟกระชาก
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// MAC Address ของ Anchor 3
char anchor_addr[] = "86:00:00:00:00:00:00:03";

// 🎯 ใส่ค่า Adelay ที่ได้จาก Anchor ตัวก่อนหน้าเป็นค่าเริ่มต้น
uint16_t Adelay = 16570; 

// Pin Configuration (อิงจากโค้ดเดิมของคุณ) [cite: 110, 111]
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS    5

const uint8_t PIN_RST = 22;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = 5;

float sum_distance = 0;
int read_count = 0;

void setup() {
  // ปิดระบบป้องกันไฟตก (Brownout) เพื่อไม่ให้บอร์ดรีเซ็ตเองรัวๆ
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ANCHOR 3 CALIBRATION MODE ===");
  
  // เริ่มต้น SPI โดยไม่ส่ง CS เข้าไปใน begin เพื่อกันบอร์ดค้าง [cite: 99]
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
  // 🛑 จุดสำคัญ: ตรวจสอบก่อนว่ามีข้อมูลอุปกรณ์ไหม เพื่อป้องกันบอร์ดแครช
  DW1000Device *device = DW1000Ranging.getDistantDevice();
  if (device == NULL) return; 

  float d = device->getRange();
  if (isnan(d) || d <= 0) return;

  sum_distance += d;
  read_count++;

  if (read_count >= 10) {
    float avg_d = sum_distance / 10.0;
    
    Serial.print("Adelay: "); Serial.print(Adelay);
    Serial.print(" | ระยะ: "); Serial.print(avg_d, 2);
    Serial.print(" m");

    // เป้าหมาย 3.15 เมตรตามระยะจริงที่คุณวางไว้
    if (avg_d > 3.18) {
      Serial.println("  => [ลด] ค่า Adelay ลงนิดนึง");
    } else if (avg_d < 3.12) {
      Serial.println("  => [เพิ่ม] ค่า Adelay ขึ้นนิดนึง");
    } else {
      Serial.println("  => ✅ นิ่งแล้ว! จดค่า Adelay นี้ไว้ใช้");
    }

    sum_distance = 0;
    read_count = 0;
  }
}