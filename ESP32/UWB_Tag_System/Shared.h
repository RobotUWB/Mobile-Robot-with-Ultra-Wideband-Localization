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
static const uint8_t PIN_SS  = 5;

// =================== FIELD CONSTANTS ===================
static constexpr float FIELD_W = 3.00f;
static constexpr float FIELD_H = 2.00f;

// ===== Heights (meters) =====
static constexpr float TAG_Z = 0.30f; 
static constexpr float AZ[4] = { 1.60f, 1.60f, 1.60f, 1.60f }; 

static constexpr bool USE_2D_HEIGHT_CORR = true;

// =================== ANCHOR POSITIONS ===================
static constexpr float AX[4] = { 0.0f, FIELD_W, 0.0f, FIELD_W };
static constexpr float AY[4] = { FIELD_H, FIELD_H, 0.0f, 0.0f };

// =================== RANGE VALIDATION ===================
static constexpr float MIN_RANGE_M = 0.10f; 
static constexpr float MAX_RANGE_M = 10.00f;

// =================== FILTERING ===================
static constexpr int MED_N = 7;
static constexpr float MAX_JUMP_M = 5.00f;
static constexpr float RANGE_EMA_ALPHA = 0.10f;

// =================== XY SMOOTHING ===================
static constexpr float XY_ALPHA_BASE = 0.15f; 
static constexpr float XY_ALPHA_MIN  = 0.05f; 
static constexpr float XY_ALPHA_MAX  = 0.40f; 

// =================== POSITION GATING ===================
static constexpr float RMSE_GATE_M = 0.40f; 
static constexpr float RMSE_HARD_M = 0.80f; 
static constexpr float MAX_STEP_M = 0.50f;  

// =================== TIMING ===================
static constexpr uint32_t ANCHOR_TIMEOUT_MS = 1000;
static constexpr uint32_t CAL_MS = 5000;

// =================== STRUCTS & GLOBALS ===================
struct Meas { float ax, ay; float r; int id; };

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

extern bool cal_active;
extern bool cal_multi_point;
extern float cal_ref_x, cal_ref_y;
extern uint32_t cal_start_ms;
extern double cal_sum[4];
extern int cal_cnt[4];
extern double mp_bias_sum[4];
extern int mp_bias_cnt[4];

// >>> NEW: Status Variable (0=Idle, 1=Calibrating, 2=Success, 3=Fail, 4=Reset)
extern int cal_state;

// =================== SHARED HELPER FUNCTIONS ===================
void saveBias();
void loadBias();
bool validRange(float d);
float hypot2(float dx, float dy);
String anchorString(int idx);

#endif