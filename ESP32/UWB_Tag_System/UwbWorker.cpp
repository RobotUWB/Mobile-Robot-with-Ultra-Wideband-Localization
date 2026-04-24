#include "UwbWorker.h"
#include "Shared.h"
#include "DW1000Ranging.h"
#include "WebWorker.h" // activeIP()

static bool is_saving = false; // ตัวแปรป้องกันการทำงานซ้อนกัน

// =================== Local config ===================
static const uint32_t PRINT_MS = 400;
static const int      GN_ITERS = 8; 
static const float    GN_EPS   = 1e-4f;

// Median ring buffer per anchor
static float buf[4][MED_N];
static int   buf_i[4]    = {0,0,0,0};
static int   buf_fill[4] = {0,0,0,0};

// =================== SMALL HELPERS ===================
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

bool validRange(float d) {
  return (!isnan(d)) && (d >= MIN_RANGE_M && d <= MAX_RANGE_M);
}

float hypot2(float dx, float dy) {
  return sqrtf(dx*dx + dy*dy);
}

String anchorString(int idx) {
  if (aStat[idx] == -2) return String("-2");         
  if (!validRange(fA[idx])) return String("-1");     
  float v = fA[idx]; 
  return String(v, 2);
}

// EMA update for RANGE (per anchor)
static inline float emaUpdateRange(float prev, float x) {
  if (!validRange(prev)) return x;
  return prev + RANGE_EMA_ALPHA * (x - prev);
}

// 3D -> 2D flatten
static inline bool flattenRange2D(int id, float r3, float &r2) {
  if (!validRange(r3)) return false;
  if (!USE_2D_HEIGHT_CORR) { r2 = r3; return true; }

  const float dz  = AZ[id] - TAG_Z;
  const float r32 = r3 * r3;
  const float dz2 = dz * dz;

  if (r32 <= dz2) { 
    r2 = 0.01f; 
    return true; 
  }

  r2 = sqrtf(r32 - dz2);
  if (r2 < 0.01f) r2 = 0.01f; 

  return true;
}

static int idFromShort(uint16_t sa) {
  if (sa == 0x84) return 0; // SA_A1
  if (sa == 0x85) return 1; // SA_A2
  if (sa == 0x86) return 2; // SA_A3
  if (sa == 0x87) return 3; // SA_A4
  return -1;
}

static float medianN(const float *a, int n) {
  float t[MED_N];
  for (int i = 0; i < n; i++) t[i] = a[i];
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (t[j] < t[i]) {
        float tmp = t[i]; t[i] = t[j]; t[j] = tmp;
      }
    }
  }
  return t[n / 2];
}

// =================== STORAGE ===================
void loadBias() {
  prefs.begin("uwbcal1", true);
  bias[0] = prefs.getFloat("b1", 0.0f);
  bias[1] = prefs.getFloat("b2", 0.0f);
  bias[2] = prefs.getFloat("b3", 0.0f);
  bias[3] = prefs.getFloat("b4", 0.0f);
  prefs.end();
}

void saveBias() {
  prefs.begin("uwbcal1", false);
  prefs.putFloat("b1", bias[0]);
  prefs.putFloat("b2", bias[1]);
  prefs.putFloat("b3", bias[2]);
  prefs.putFloat("b4", bias[3]);
  prefs.end();
}

// =================== POSITION SMOOTHING ===================
static float computePosAlpha(int n, float rmse) {
  float a = XY_ALPHA_BASE;
  if (n >= 4) a += 0.05f;
  else if (n == 3) a -= 0.03f;

  if (rmse >= 0) {
    if (rmse < 0.03f)      a += 0.18f;
    else if (rmse < 0.06f) a += 0.10f;
    else if (rmse < 0.10f) a += 0.04f;
    else if (rmse > 0.18f) a -= 0.14f;
    else if (rmse > 0.12f) a -= 0.08f;
  }
  return clampf(a, XY_ALPHA_MIN, XY_ALPHA_MAX);
}

// =================== MATH / SOLVER ===================
static bool solveXY_GN(const Meas *m, int n, float &x, float &y, float &rmse, float x0, float y0) {
  if (n < 3) return false;
  float xx = x0;
  float yy = y0;

  for (int it = 0; it < GN_ITERS; ++it) {
    float a11 = 0, a12 = 0, a22 = 0;
    float b1  = 0, b2  = 0;

    for (int i = 0; i < n; i++) {
      float dx = xx - m[i].ax;
      float dy = yy - m[i].ay;
      float di = sqrtf(dx * dx + dy * dy);
      if (di < 1e-6f) di = 1e-6f;

      float ri = di - m[i].r;
      float jx = dx / di;
      float jy = dy / di;

      a11 += jx * jx;
      a12 += jx * jy;
      a22 += jy * jy;
      b1  += jx * ri;
      b2  += jy * ri;
    }

    float det = a11 * a22 - a12 * a12;
    if (fabs(det) < 1e-9f) break;

    float dxx = (a22 * b1 - a12 * b2) / det;
    float dyy = (-a12 * b1 + a11 * b2) / det;

    xx -= dxx;
    yy -= dyy;

    if (fabs(dxx) < GN_EPS && fabs(dyy) < GN_EPS) break;
  }

  float sse = 0;
  for (int i = 0; i < n; i++) {
    float di = hypot2(xx - m[i].ax, yy - m[i].ay);
    float ri = di - m[i].r;
    sse += ri * ri;
  }

  rmse = sqrtf(sse / n);
  x = xx;
  y = yy;
  return true;
}

static int worstResidualIndex(const Meas *m, int n, float x, float y) {
  int wi = -1;
  float wabs = -1;
  for (int i = 0; i < n; i++) {
    float di = hypot2(x - m[i].ax, y - m[i].ay);
    float ri = di - m[i].r;
    float a  = fabs(ri);
    if (a > wabs) { wabs = a; wi = i; }
  }
  return wi;
}

// =================== DW1000 CALLBACKS ===================
void newRange() {
  DW1000Device *dev = DW1000Ranging.getDistantDevice();
  if (!dev) return;

  uint16_t sa = dev->getShortAddress();
  int id = idFromShort(sa);
  if (id < 0 || id >= 4) return;

  float d = dev->getRange();
  if (!validRange(d)) {
    aStat[id] = -1;
    return;
  }

  float d_corr = d - bias[id];
  if (!validRange(d_corr)) {
    aStat[id] = -1;
    return;
  }

  // DEBUG
  //static uint32_t dbgMs = 0;
  //if (millis() - dbgMs > 100) {
    //dbgMs = millis();
    //Serial.printf("[DBG] id=%d raw=%.3f bias=%.3f corr=%.3f\n", id, d, bias[id], d_corr);
  //}

  buf[id][buf_i[id]] = d_corr;
  buf_i[id] = (buf_i[id] + 1) % MED_N;
  if (buf_fill[id] < MED_N) buf_fill[id]++;

  float d_med = (buf_fill[id] >= 3) ? medianN(buf[id], buf_fill[id]) : d_corr;
  if (!validRange(d_med)) {
    aStat[id] = -1;
    return;
  }

  // jump reject
  if (validRange(lastAccepted[id])) {
    if (fabs(d_med - lastAccepted[id]) > MAX_JUMP_M) {
      aStat[id] = -2;

      // clear anchor นี้ออกไปก่อนไม่ใช้ค่าเก่า
      fA[id] = -1;
      lastAccepted[id] = -1;
      tA[id] = 0;
      buf_fill[id] = 0;
      buf_i[id] = 0;

      return;
    }
  }

  // accept + EMA
  aStat[id] = 0;
  lastAccepted[id] = d_med;
  fA[id] = emaUpdateRange(fA[id], d_med);
  tA[id] = millis();

  if (cal_active && validRange(fA[id])) {
    cal_sum[id] += fA[id];
    cal_cnt[id] += 1;
  }
}

void newDevice(DW1000Device *device) {
  uint16_t sa = device->getShortAddress();
  int id = idFromShort(sa);
  Serial.printf("[TAG1] Connected anchor SA=0x%04X -> id=%d\n", sa, id);

  if (id >= 0 && id < 4) {
    // reset filter/state ของ anchor ตัวนี้ตอน reconnect
    fA[id] = -1;
    lastAccepted[id] = -1;
    tA[id] = 0;
    aStat[id] = 0;
    buf_fill[id] = 0;
    buf_i[id] = 0;
  }
}

void inactiveDevice(DW1000Device *device) {
  uint16_t sa = device->getShortAddress();
  int id = idFromShort(sa);
  Serial.printf("[TAG1] Anchor inactive SA=0x%04X -> id=%d\n", sa, id);

  if (id >= 0 && id < 4) {
    // clear ค่า range เก่า
    fA[id] = -1;
    lastAccepted[id] = -1;
    tA[id] = 0;
    aStat[id] = -1;
    buf_fill[id] = 0;
    buf_i[id] = 0;
  }
}

// =================== SERIAL CMD ===================
void handleSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line.startsWith("CALP")) {
    float x, y;
    if (sscanf(line.c_str(), "CALP %f %f", &x, &y) == 2) {
      cal_ref_x = x; cal_ref_y = y; cal_active = true; cal_multi_point = true;
      cal_start_ms = millis(); cal_state = 1; 
      for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
      Serial.printf("[CALP] start ref=(%.2f,%.2f)\n", cal_ref_x, cal_ref_y);
    } 
    return;
  }

  if (line.startsWith("CAL")) {
    float x, y;
    if (sscanf(line.c_str(), "CAL %f %f", &x, &y) == 2) {
      cal_ref_x = x; cal_ref_y = y; cal_active = true; cal_multi_point = false;
      cal_start_ms = millis(); cal_state = 1; 
      for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
      Serial.printf("[CAL] start ref=(%.2f,%.2f)\n", cal_ref_x, cal_ref_y);
    }
    return;
  }

  if (line == "SAVE") {
    is_saving = true; delay(200);
    for (int i = 0; i < 4; i++) if (mp_bias_cnt[i] > 0) bias[i] = mp_bias_sum[i] / mp_bias_cnt[i];
    saveBias(); delay(500);
    Serial.println("[CAL] Saved! Restarting...");
    delay(1000); ESP.restart();
    return;
  }

  if (line == "RESET") {
    cal_active = false;
    for (int i = 0; i < 4; i++) {
      bias[i] = 0;
      mp_bias_sum[i] = 0;
      mp_bias_cnt[i] = 0;
      fA[i] = -1;
      lastAccepted[i] = -1;
      tA[i] = 0;
      aStat[i] = -1;
      buf_fill[i] = 0;
      buf_i[i] = 0;
    }
    saveBias(); cal_state = 4;
    Serial.println("[CAL] Reset done.");
    return;
  }
}

// =================== SETUP UWB ===================
void setupUWB() {
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  char tag_addr[] = "7D:00:22:EA:82:60:3B:9C";
  
  // ใช้ Mode LONGDATA_RANGE_LOWPOWER ซึ่งเป็นโหมดมาตรฐานสำหรับระยะไกล
  // (Data Rate 110kbps, Preamble 2048) -> ทะลุสิ่งกีดขวางได้ดีที่สุดในโหมดมาตรฐาน
  DW1000Ranging.startAsTag(tag_addr, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);

  // [REMOVED] เอาคำสั่ง setManualTxPower ที่ error ออก
  // DW1000.setManualTxPower(...) 
  // DW1000Ranging.useSmartPower(...)

  loadBias();

  Serial.println("=== TAG1 READY (Standard Long Range Mode) ===");
}

// =================== LOOP UWB ===================
void loopUWB() {
  if (is_saving) { delay(1); return; }

  DW1000Ranging.loop();
  handleSerial();

  // Calibration Logic
  if (cal_active && (millis() - cal_start_ms > CAL_MS)) {
    cal_active = false;
    float ex[4];
    for (int i = 0; i < 4; i++) {
      float dx = cal_ref_x - AX[i]; float dy = cal_ref_y - AY[i]; float dz = TAG_Z - AZ[i]; 
      ex[i] = sqrtf(dx*dx + dy*dy + dz*dz); 
    }

    float newBias[4] = { bias[0], bias[1], bias[2], bias[3] };
    for (int i = 0; i < 4; i++) {
      if (cal_cnt[i] > 5) { 
        float avg = (float)(cal_sum[i] / (double)cal_cnt[i]); 
        newBias[i] = (avg + bias[i]) - ex[i];
      }
    }

    if (!cal_multi_point) {
      for (int i = 0; i < 4; i++) bias[i] = newBias[i];
      Serial.println("[CAL] Finished. Type SAVE.");
    } else {
      for (int i = 0; i < 4; i++) { mp_bias_sum[i] += newBias[i]; mp_bias_cnt[i] += 1; }
      Serial.printf("[CALP] Point %d saved.\n", mp_bias_cnt[0]);
    }
    cal_state = 2; 
  }

  // Anchor timeout
  uint32_t now = millis();
  for (int i = 0; i < 4; ++i) {
    if (tA[i] != 0 && (now - tA[i] > ANCHOR_TIMEOUT_MS)) {
      fA[i] = -1;
      lastAccepted[i] = -1;
      tA[i] = 0;
      aStat[i] = -1;
      buf_fill[i] = 0;
      buf_i[i] = 0;
    }
  }

  // Position Calculation
  Meas m[4];
  int n = 0;
  for (int i = 0; i < 4; i++) {
    if (!validRange(fA[i])) continue;
    float r3 = fA[i] < MIN_RANGE_M ? MIN_RANGE_M : fA[i];
    float r2;
    if (!flattenRange2D(i, r3, r2)) continue; 
    if (r2 < MIN_RANGE_M) r2 = MIN_RANGE_M;
    m[n].ax = AX[i]; m[n].ay = AY[i]; m[n].r = r2; m[n].id = i;
    n++;
  }

  float x_raw = -1, y_raw = -1, rmse = 999;
  bool okSolve = false;
  float x0 = have_xy ? x_f : FIELD_W * 0.5f;
  float y0 = have_xy ? y_f : FIELD_H * 0.5f;

  if (n >= 3) {
    okSolve = solveXY_GN(m, n, x_raw, y_raw, rmse, x0, y0);
    if (okSolve && n == 4 && rmse > RMSE_GATE_M) {
      int wi = worstResidualIndex(m, n, x_raw, y_raw);
      if (wi >= 0) {
        Meas m2[3]; int k = 0;
        for (int i = 0; i < n; i++) if (i != wi) m2[k++] = m[i];
        float x2, y2, rmse2;
        if (solveXY_GN(m2, 3, x2, y2, rmse2, x_raw, y_raw)) {
           if (rmse2 < rmse) { x_raw = x2; y_raw = y2; rmse = rmse2; }
        }
      }
    }
  }

  bool acceptWeak = (n >= 3) && okSolve && (rmse <= RMSE_HARD_M);
  last_accept = acceptWeak; last_n = n; last_rmse = okSolve ? rmse : -1;

  if (acceptWeak) {
    float alpha = computePosAlpha(n, rmse);
    if (!have_xy) { x_f = x_raw; y_f = y_raw; have_xy = true; } 
    else { x_f = x_f + alpha * (x_raw - x_f); y_f = y_f + alpha * (y_raw - y_f); }
  }

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= PRINT_MS) {
    lastPrint = millis();
    Serial.printf("[TAG1] n=%d A1=%s A2=%s A3=%s A4=%s | x=%.2f y=%.2f | open=http://%s/\n",
                  n, anchorString(0).c_str(), anchorString(1).c_str(), anchorString(2).c_str(), anchorString(3).c_str(),
                  have_xy ? x_f : -1.0f, have_xy ? y_f : -1.0f,
                  activeIP().toString().c_str());
  }
}