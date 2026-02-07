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
// ปรับค่าเพื่อให้สนามยาว 3 เมตรในแนวนอน และกว้าง 2 เมตรในแนวตั้ง
static constexpr float FIELD_W = 3.00f; // ความยาวสนาม (แนวนอน)
static constexpr float FIELD_H = 2.00f; // ความกว้างสนาม (แนวตั้ง)

// ===== Heights (meters) =====
// ตั้งค่าความสูง Tag ที่ 0.30 เมตร ตามโครงสร้างหุ่นยนต์จริง
static constexpr float TAG_Z = 0.315f; // ความสูง Tag Robot

// หากพิกัดใน Dashboard หดเข้ากลางสนาม ให้ลองปรับค่า AZ ลดลงเหลือ 1.45f หรือ 1.50f
static constexpr float AZ[4] = { 1.00f, 1.00f, 1.00f, 1.00f }; //ความสูงเสา

static constexpr bool USE_2D_HEIGHT_CORR = true;

// ตรวจสอบพิกัดให้เป็น A1(0,0), A2(0,2), A3(3,0), A4(3,2)
static constexpr float AX[4] = { 0.00f, 0.00f, 3.00f, 3.00f };
static constexpr float AY[4] = { 0.00f, 2.00f, 0.00f, 2.00f };

// =================== RANGE VALIDATION ===================
static constexpr float MIN_RANGE_M = 0.10f;
static constexpr float MAX_RANGE_M = 15.00f;

// =================== FILTERING ===================+
static constexpr int MED_N = 3; // ลดจาก 5 เหลือ 3 เพื่อความเร็ว
static constexpr float MAX_JUMP_M = 3.00f;

// เพิ่มค่า Alpha เป็น 0.30 เพื่อให้การตอบสนองระยะทาง Real-time มากขึ้น
static constexpr float RANGE_EMA_ALPHA = 0.30f;

// =================== XY SMOOTHING ===================
// เพิ่มความไวพิกัด X, Y เป็น 0.25 เพื่อให้จุดวิ่งตามหุ่นยนต์ทันที
static constexpr float XY_ALPHA_BASE = 0.25f;
static constexpr float XY_ALPHA_MIN  = 0.10f;
static constexpr float XY_ALPHA_MAX  = 0.60f;

// =================== POSITION GATING ===================
static constexpr float RMSE_GATE_M = 1.00f;
static constexpr float RMSE_HARD_M = 2.00f;
static constexpr float MAX_STEP_M = 0.50f; 

// =================== TIMING ===================
// ตั้งค่า Timeout ไว้ที่ 2.5 วินาที เพื่อความเสถียรของการเชื่อมต่อ
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