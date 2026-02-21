#include <SPI.h>
#include "DW1000Ranging.h"
#include "DW1000.h"

char anchor_addr[] = "87:00:00:00:00:00:00:04";
uint16_t Adelay = 16464;

#define SPI_SCK  18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS    5

const uint8_t PIN_RST = 22;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS  = DW_CS;

static const float MIN_RANGE_M = 0.05f;
static const float MAX_RANGE_M = 10.0f;
static const float MAX_JUMP_M  = 0.50f;
static const float EMA_ALPHA   = 0.25f;
static const uint32_t LOG_INTERVAL_MS = 200;

static bool   have_last = false;
static float  last_ok   = NAN;
static float  ema_range = NAN;
static uint32_t last_log = 0;

static inline bool validRange(float d) {
  return (!isnan(d)) && d >= MIN_RANGE_M && d <= MAX_RANGE_M;
}

void newRange();
void newDevice(DW1000Device *device);
void inactiveDevice(DW1000Device *device);

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("### ANCHOR A4 START ###");
  Serial.print("EUI: "); Serial.println(anchor_addr);
  Serial.print("Antenna delay: "); Serial.println(Adelay);

  // ✅ ชัวร์สุด: ไม่ส่ง CS เข้า SPI.begin
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  pinMode(DW_CS, OUTPUT);
  digitalWrite(DW_CS, HIGH);

  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  DW1000.setAntennaDelay(Adelay);

  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  DW1000Ranging.startAsAnchor(anchor_addr, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);
}

void loop() {
  DW1000Ranging.loop();
}

void newRange() {
  uint16_t tag = DW1000Ranging.getDistantDevice()->getShortAddress();
  float d = DW1000Ranging.getDistantDevice()->getRange();
  if (!validRange(d)) return;

  if (have_last && validRange(last_ok)) {
    if (fabs(d - last_ok) > MAX_JUMP_M) return;
  }
  last_ok = d;
  have_last = true;

  if (isnan(ema_range)) ema_range = d;
  else ema_range = ema_range + EMA_ALPHA * (d - ema_range);

  uint32_t now = millis();
  if (now - last_log < LOG_INTERVAL_MS) return;
  last_log = now;

  Serial.print("[ANCHOR A4] TAG 0x");
  Serial.print(tag, HEX);
  Serial.print(" -> ");
  Serial.print(ema_range, 2);
  Serial.println(" m");
}

static void resetFilterState() {
  have_last = false;
  last_ok = NAN;
  ema_range = NAN;
}

void newDevice(DW1000Device *device) {
  Serial.print("[ANCHOR A4] Connected TAG: 0x");
  Serial.println(device->getShortAddress(), HEX);

  resetFilterState();
}

void inactiveDevice(DW1000Device *device) {
  Serial.print("[ANCHOR A4] TAG inactive: 0x");
  Serial.println(device->getShortAddress(), HEX);
  
  resetFilterState();
}
