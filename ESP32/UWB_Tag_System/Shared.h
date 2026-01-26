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
static constexpr float FIELD_W = 2.00f; 
static constexpr float FIELD_H = 3.00f; 

// ===== Heights (meters) =====
// [แก้ไข 1] ตั้งค่า Tag สูง 0.30 เมตร (ตามความจริงบนหุ่นยนต์)
static constexpr float TAG_Z = 0.30f; 

// [เทคนิคแก้จุดไม่เข้ามุม]
// ค่าจริงคือ 1.60f แต่ถ้าเดินแล้วจุดมันหดเข้ากลาง (ไปไม่ถึงขอบ)
// ให้ลองลดค่าตรงนี้ลงเหลือ 1.50f หรือ 1.45f เพื่อช่วยดันจุดให้ออกกว้างขึ้นครับ
static constexpr float AZ[4] = { 1.60f, 1.60f, 1.60f, 1.60f }; 

static constexpr bool USE_2D_HEIGHT_CORR = true;

// =================== ANCHOR POSITIONS ===================
static constexpr float AX[4] = { 0.0f, FIELD_W, 0.0f, FIELD_W };
static constexpr float AY[4] = { FIELD_H, FIELD_H, 0.0f, 0.0f };

// =================== RANGE VALIDATION ===================
static constexpr float MIN_RANGE_M = 0.10f; 
static constexpr float MAX_RANGE_M = 15.00f;

// =================== FILTERING ===================
static constexpr int MED_N = 5; 
static constexpr float MAX_JUMP_M = 3.00f; 

// [แก้ไข 2] ใช้ค่า Alpha สูง (0.30) เพื่อให้ตำแหน่ง Real-time
// แก้ปัญหากระตุก/วาป ทำให้จุดวิ่งตามหุ่นทันที
static constexpr float RANGE_EMA_ALPHA = 0.30f; 

// =================== XY SMOOTHING ===================
// [แก้ไข 3] เพิ่มความไวพิกัด X,Y เป็น 0.25 (ค่าเดิม 0.05 มันช้าไปสำหรับหุ่นยนต์)
static constexpr float XY_ALPHA_BASE = 0.25f; 
static constexpr float XY_ALPHA_MIN  = 0.10f; 
static constexpr float XY_ALPHA_MAX  = 0.60f; 

// =================== POSITION GATING ===================
static constexpr float RMSE_GATE_M = 1.00f; 
static constexpr float RMSE_HARD_M = 2.00f; 
static constexpr float MAX_STEP_M = 0.50f;  

// =================== TIMING ===================
// ลด Timeout เหลือ 2.5 วินาที เพื่อให้ระบบตื่นตัวตลอดเวลา
static constexpr uint32_t ANCHOR_TIMEOUT_MS = 2500;
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

extern int cal_state;

extern float t2_x, t2_y;
extern uint32_t t2_last_ms;

// =================== SHARED HELPER FUNCTIONS ===================
void saveBias();
void loadBias();
bool validRange(float d);
float hypot2(float dx, float dy);
String anchorString(int idx);

#endif