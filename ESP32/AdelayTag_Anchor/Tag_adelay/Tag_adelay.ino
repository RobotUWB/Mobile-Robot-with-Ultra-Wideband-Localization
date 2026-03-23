#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

// MAC Address ของ Tag
char tag_addr[] = "7D:00:22:EA:82:60:3B:9C";

// Pin Configuration (อิงจากบอร์ดเดิม)
#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS    5

const uint8_t PIN_RST = 22;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = 5;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== TAG IS READY FOR CALIBRATION ===");

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, DW_CS);
  pinMode(DW_CS, OUTPUT);
  digitalWrite(DW_CS, HIGH);

  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  
  // ให้ Tag ใช้โหมดเดียวกับโค้ดหลัก
  DW1000Ranging.startAsTag(tag_addr, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);
}

void loop() {
  DW1000Ranging.loop();
}