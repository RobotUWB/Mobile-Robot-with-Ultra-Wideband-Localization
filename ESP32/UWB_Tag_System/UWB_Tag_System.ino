#include "Shared.h"
#include "UwbWorker.h"
#include "WebWorker.h"
#include <esp_now.h>
#include <WiFi.h>

// =================== GLOBAL VARIABLE DEFINITIONS ==================
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

// [CRITICAL] ประกาศ Mutex ที่นี่ (Global)
portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;

// --- ESP-NOW Vars ---
volatile float isr_t2_x = 0.0f;
volatile float isr_t2_y = 0.0f;
volatile uint32_t isr_t2_last_ms = 0;
volatile bool isr_new_data = false;

// WebWorker Shared Vars
float t2_x = 0.0f;
float t2_y = 0.0f;
uint32_t t2_last_ms = 0;

typedef struct struct_message {
  int id;       
  float x;
  float y;      
} struct_message;

struct_message incomingData;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingDataPtr, int len) {
  if (len != sizeof(incomingData)) return;
  memcpy(&incomingData, incomingDataPtr, sizeof(incomingData));
  if (incomingData.id == 2) {
    isr_t2_x = incomingData.x;
    isr_t2_y = incomingData.y;
    isr_t2_last_ms = millis();
    isr_new_data = true; 
  }
}

// =================== MAIN SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(500); 

  // 1. Setup Web & WiFi 
  setupWeb();

  Serial.println("-------------------------------------");
  Serial.print("[TAG 1] WiFi SSID: "); Serial.println(WiFi.SSID());
  Serial.print("[TAG 1] WiFi Channel: "); Serial.println(WiFi.channel());
  Serial.println("-------------------------------------");
  
  // เริ่ม ESP-NOW หลังจาก WiFi
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
    Serial.println("[ESP-NOW] Init Success");
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
    portENTER_CRITICAL(&myMutex);
    t2_x = isr_t2_x;
    t2_y = isr_t2_y;
    t2_last_ms = isr_t2_last_ms;
    isr_new_data = false;
    portEXIT_CRITICAL(&myMutex);
  }

  // [แก้ไข] Priority Logic: ให้ UWB ทำงานหนักกว่า Web
  // เพื่อป้องกันไม่ให้ UWB ขาดช่วงจน Anchor หลุด
  
  loopUWB(); // ทำงานทุกรอบ (Real-time)

  // Web/WiFi ทำงานแค่ทุกๆ 20ms (50Hz) ก็พอ
  static uint32_t last_web_poll = 0;
  if (millis() - last_web_poll > 50) {
      last_web_poll = millis();
      loopWeb();
  }
}