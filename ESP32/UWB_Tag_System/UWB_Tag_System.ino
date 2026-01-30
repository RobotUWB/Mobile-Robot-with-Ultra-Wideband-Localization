#include "Shared.h"
#include "UwbWorker.h"
#include "WebWorker.h"
#include <esp_now.h>
#include <WiFi.h>

// =================== GLOBAL VARIABLE DEFINITIONS ===================
Preferences prefs;
float bias[4] = { 0, 0, 0, 0 };

// Runtime state
float fA[4] = { -1, -1, -1, -1 };
float lastAccepted[4] = { -1, -1, -1, -1 };
uint32_t tA[4] = { 0, 0, 0, 0 };
int8_t aStat[4] = { -1, -1, -1, -1 };

// Position state
bool have_xy = false;
float x_f = FIELD_W * 0.5f, y_f = FIELD_H * 0.5f;
float last_rmse = -1;
bool last_accept = false;
int last_n = 0;

// Alerts / status
bool tag_lost = true;
uint32_t lost_since_ms = 0;
bool out_of_field = false;
bool out_of_zone  = false;

// Calibration state
bool cal_active = false;
bool cal_multi_point = false;
float cal_ref_x = FIELD_W * 0.5f, cal_ref_y = FIELD_H * 0.5f;
uint32_t cal_start_ms = 0;
double cal_sum[4] = { 0, 0, 0, 0 };
int cal_cnt[4] = { 0, 0, 0, 0 };
double mp_bias_sum[4] = { 0, 0, 0, 0 };
int mp_bias_cnt[4] = { 0, 0, 0, 0 };
int cal_state = 0; 

// [แก้ไข] ประกาศ Mutex ที่นี่ (Global) เพื่อให้ใช้ร่วมกันได้จริง
portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

// --- ตัวแปร volatile สำหรับรับค่าจาก ESP-NOW ---
// ใช้ในฟังก์ชัน OnDataRecv เท่านั้น เพื่อความเสถียร
volatile float isr_t2_x = 0.0f;
volatile float isr_t2_y = 0.0f;
volatile uint32_t isr_t2_last_ms = 0;
volatile bool isr_new_data = false;

// ตัวแปร Global จริงที่ WebWorker จะมาดึงไปใช้ (ประกาศไว้ใน Shared.h)
float t2_x = 0.0f;
float t2_y = 0.0f;
uint32_t t2_last_ms = 0;

// โครงสร้างข้อมูลต้องตรงกับ Tag 2
typedef struct struct_message {
  int id;       
  float x;
  float y;      
} struct_message;

struct_message incomingData;

// ฟังก์ชัน Callback เมื่อได้รับข้อมูลจาก Tag 2
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingDataPtr, int len) {
  
  // [SAFETY CHECK] ตรวจสอบขนาดข้อมูลก่อน เพื่อป้องกันข้อมูลขยะ
  if (len != sizeof(incomingData)) {
    return; // ถ้าขนาดไม่ตรง ให้ข้ามไปเลย
  }

  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));

  // ตรวจสอบว่าเป็น ID 2 หรือไม่
  if (incomingData.id == 2) {
    // อัปเดตลงตัวแปร volatile
    isr_t2_x = incomingData.x;
    isr_t2_y = incomingData.y;
    isr_t2_last_ms = millis();
    isr_new_data = true; // แจ้ง main loop ว่ามีของใหม่มา
  }
}

// =================== MAIN SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(500); // รอสักนิดให้ Serial พร้อม

  // 1. Setup Web & WiFi (Must be first to set Channel)
  setupWeb();

  Serial.println("-------------------------------------");
  Serial.print("[TAG 1] WiFi SSID: "); Serial.println(WiFi.SSID());
  Serial.print("[TAG 1] WiFi Channel: "); Serial.println(WiFi.channel());
  Serial.println("-------------------------------------");
  // หมายเหตุ: Tag 2 ต้องรันบน Channel เดียวกันนี้
  
  // เริ่ม ESP-NOW หลังจาก WiFi
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
    Serial.println("[ESP-NOW] Init Success - Waiting for Tag 2...");
  } else {
    Serial.println("[ESP-NOW] Init Failed!");
  }

  // 2. Setup UWB
  setupUWB();
}

// =================== MAIN LOOP ===================
void loop() {
  // --- ย้ายค่าจาก ISR เข้าตัวแปรหลักอย่างปลอดภัย ---
  if (isr_new_data) {
    // [CRITICAL SECTION] ป้องกันข้อมูลตีกันระหว่าง Interrupt และ Main Loop
    // [แก้ไข] ใช้ตัวแปร Global myMutex แทนการประกาศใหม่
    portENTER_CRITICAL(&myMutex);

    t2_x = isr_t2_x;
    t2_y = isr_t2_y;
    t2_last_ms = isr_t2_last_ms;
    isr_new_data = false;

    portEXIT_CRITICAL(&myMutex);
  }

  loopUWB();
  loopWeb();
  delay(1); 
}