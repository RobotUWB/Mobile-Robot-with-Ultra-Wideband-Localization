#ifndef SHARED_H
#define SHARED_H

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>

// =================== PIN DEFINITIONS ===================
#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS 5

static const uint8_t PIN_RST = 22;
static const uint8_t PIN_IRQ = 34;
static const uint8_t PIN_SS = 5;

// =================== FIELD CONSTANTS ===================
#define FIELD_W 3.00f
#define FIELD_H 2.00f
static const float TAG_Z = 0.12f;
static const float AZ[4] = { 0.95f, 0.95f, 0.95f, 0.95f };
static const float AX[4] = { 0.0f, FIELD_W, 0.0f, FIELD_W };
static const float AY[4] = { FIELD_H, FIELD_H, 0.0f, 0.0f };
static const bool USE_2D_HEIGHT_CORR = true;

// =================== LOGIC CONSTANTS ===================
static const float MIN_RANGE_M = 0.15f;
static const float MAX_RANGE_M = 10.0f;
static const int MED_N = 7;
static const float EMA_ALPHA = 0.15f;
static const float MAX_JUMP_M = 0.60f;
static const float RMSE_GOOD = 0.12f;
static const float RMSE_MAX = 0.22f;
static const float XY_ALPHA_BASE = 0.22f;
static const uint32_t ANCHOR_TIMEOUT_MS = 900;
static const uint32_t CAL_MS = 4500;

// =================== STRUCTS ===================
struct Meas {
  float ax, ay;
  float r;
  int id;
};

// =================== GLOBAL VARS (EXTERN) ===================
// These are defined in UWB_Tag_System.ino, but accessible everywhere
extern Preferences prefs;
extern float bias[4];
extern float fA[4];
extern float lastAccepted[4];
extern uint32_t tA[4];
extern int8_t aStat[4];

extern bool have_xy;
extern float x_f, y_f;
extern float last_rmse;
extern bool last_accept;
extern int last_n;

// Calibration globals shared between Serial (UWB) and Web
extern bool cal_active;
extern bool cal_multi_point;
extern float cal_ref_x, cal_ref_y;
extern uint32_t cal_start_ms;
extern double cal_sum[4];
extern int cal_cnt[4];
extern double mp_bias_sum[4];
extern int mp_bias_cnt[4];

// =================== SHARED HELPER FUNCTIONS ===================
// Functions implemented in UwbWorker.cpp but needed by WebWorker
void saveBias();
void loadBias();
bool validRange(float d);
float hypot2(float dx, float dy);
String anchorString(int idx);

#endif