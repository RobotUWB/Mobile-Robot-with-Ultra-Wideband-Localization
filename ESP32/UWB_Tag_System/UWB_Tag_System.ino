#include "Shared.h"
#include "UwbWorker.h"
#include "WebWorker.h"

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

// >>> NEW: Init State
int cal_state = 0; 

// =================== MAIN SETUP ===================
void setup() {
  Serial.begin(115200);
  delay(200);
  setupWeb(); // Start WiFi/Web first
  setupUWB(); // Start DW1000
}

// =================== MAIN LOOP ===================
void loop() {
  loopUWB();
  loopWeb();
  delay(1); 
}