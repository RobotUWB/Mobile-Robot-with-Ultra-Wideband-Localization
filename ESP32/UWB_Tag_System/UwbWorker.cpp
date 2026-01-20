#include "UwbWorker.h"
#include "Shared.h"
#include "DW1000Ranging.h"
#include "WebWorker.h" // activeIP()

// =================== Local config ===================
static const uint32_t PRINT_MS = 400;
static const int      GN_ITERS = 14;
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
  if (aStat[idx] == -2) return String("-2");         // rejected by gate
  if (!validRange(fA[idx])) return String("-1");     // no data/timeout
  float v = fA[idx]; 
  return String(v, 2);
}

// EMA update for RANGE (per anchor)
static inline float emaUpdateRange(float prev, float x) {
  if (!validRange(prev)) return x;
  return prev + RANGE_EMA_ALPHA * (x - prev);
}

// 3D -> 2D flatten (horizontal distance) using known heights
static inline bool flattenRange2D(int id, float r3, float &r2) {
  if (!validRange(r3)) return false;
  if (!USE_2D_HEIGHT_CORR) { r2 = r3; return true; }

  const float dz  = AZ[id] - TAG_Z;
  const float r32 = r3 * r3;
  const float dz2 = dz * dz;

  // guard: if r3 <= dz -> sqrt negative (tag almost under anchor height)
  if (r32 <= dz2 + 1e-4f) return false;

  r2 = sqrtf(r32 - dz2);

  // guard: too close tends to multipath
  if (r2 < 0.25f) return false;
  return true;
}

static int idFromShort(uint16_t sa) {
  // SA Mapping
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

static int countValidAnchors() {
  int n = 0;
  for (int i = 0; i < 4; i++)
    if (validRange(fA[i])) n++;
  return n;
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
  uint16_t sa = DW1000Ranging.getDistantDevice()->getShortAddress();
  float d = DW1000Ranging.getDistantDevice()->getRange();
  if (!validRange(d)) return;

  int id = idFromShort(sa);
  if (id < 0) return;

  float d_corr = d - bias[id];
  if (!validRange(d_corr)) return;

  // ---------- Jump gate ----------
  if (validRange(lastAccepted[id])) {
    if (fabs(d_corr - lastAccepted[id]) > MAX_JUMP_M) {
      aStat[id] = -2;
      return;
    }
  }

  aStat[id] = 0;
  lastAccepted[id] = d_corr;

  // ---------- Median ----------
  buf[id][buf_i[id]] = d_corr;
  buf_i[id] = (buf_i[id] + 1) % MED_N;
  if (buf_fill[id] < MED_N) buf_fill[id]++;

  float med = (buf_fill[id] >= 3) ? medianN(buf[id], buf_fill[id]) : d_corr;

  // ---------- EMA on range ----------
  fA[id] = emaUpdateRange(fA[id], med);
  tA[id] = millis();

  // Calibration collect
  if (cal_active && validRange(fA[id])) {
    cal_sum[id] += fA[id];
    cal_cnt[id] += 1;
  }
}

void newDevice(DW1000Device *device) {
  uint16_t sa = device->getShortAddress();
  int id = idFromShort(sa);
  Serial.printf("[TAG1] Connected anchor SA=0x%04X -> id=%d\n", sa, id);
}

void inactiveDevice(DW1000Device *device) {
  uint16_t sa = device->getShortAddress();
  int id = idFromShort(sa);
  Serial.printf("[TAG1] Anchor inactive SA=0x%04X -> id=%d\n", sa, id);
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
      cal_ref_x = x;
      cal_ref_y = y;
      cal_active = true;
      cal_multi_point = true;
      cal_start_ms = millis();
      cal_state = 1; // >>> UPDATE STATE
      for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
      Serial.printf("[CALP] start ref=(%.2f,%.2f) %u ms\n", cal_ref_x, cal_ref_y, (unsigned)CAL_MS);
      Serial.println("[CALP] ทำหลายจุด แล้วพิมพ์ SAVE");
    } else Serial.println("[CALP] usage: CALP x y");
    return;
  }

  if (line.startsWith("CAL")) {
    float x, y;
    if (sscanf(line.c_str(), "CAL %f %f", &x, &y) == 2) {
      cal_ref_x = x;
      cal_ref_y = y;
      cal_active = true;
      cal_multi_point = false;
      cal_start_ms = millis();
      cal_state = 1; // >>> UPDATE STATE
      for (int i = 0; i < 4; i++) { cal_sum[i] = 0; cal_cnt[i] = 0; }
      Serial.printf("[CAL] start ref=(%.2f,%.2f) %u ms\n", cal_ref_x, cal_ref_y, (unsigned)CAL_MS);
    } else Serial.println("[CAL] usage: CAL x y");
    return;
  }

  if (line == "SAVE") {
    for (int i = 0; i < 4; i++) {
      if (mp_bias_cnt[i] > 0) bias[i] = (float)(mp_bias_sum[i] / (double)mp_bias_cnt[i]);
    }
    saveBias();
    Serial.printf("[CAL] saved bias(m): b1=%.3f b2=%.3f b3=%.3f b4=%.3f\n", bias[0], bias[1], bias[2], bias[3]);
    return;
  }

  if (line == "RESET") {
    for (int i = 0; i < 4; i++) {
      bias[i] = 0;
      mp_bias_sum[i] = 0;
      mp_bias_cnt[i] = 0;
      lastAccepted[i] = -1;
      fA[i] = -1;
      buf_fill[i] = 0;
      buf_i[i] = 0;
    }
    saveBias();
    cal_state = 4; // >>> UPDATE STATE (Reset Done)
    Serial.println("[CAL] reset bias=0 and saved.");
    return;
  }

  if (line == "SHOW") {
    Serial.printf("[CAL] bias(m): b1=%.3f b2=%.3f b3=%.3f b4=%.3f\n", bias[0], bias[1], bias[2], bias[3]);
    Serial.printf("[CALP] mp_cnt: c1=%d c2=%d c3=%d c4=%d\n", mp_bias_cnt[0], mp_bias_cnt[1], mp_bias_cnt[2], mp_bias_cnt[3]);
    return;
  }

  Serial.println("[CMD] CAL x y | CALP x y | SAVE | RESET | SHOW");
}

// =================== SETUP UWB ===================
void setupUWB() {
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  char tag_addr[] = "7D:00:22:EA:82:60:3B:9C";
  DW1000Ranging.startAsTag(tag_addr, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);

  loadBias();

  Serial.println("=== TAG1 ready (with Web Dashboard) ===");
  Serial.printf("Field: %.2fx%.2f m\n", FIELD_W, FIELD_H);
  Serial.printf("Anchors: A1(%.2f,%.2f) A2(%.2f,%.2f) A3(%.2f,%.2f) A4(%.2f,%.2f)\n",
                AX[0], AY[0], AX[1], AY[1], AX[2], AY[2], AX[3], AY[3]);
  Serial.printf("HeightCorr: %s | TAG_Z=%.2f | AZ=[%.2f,%.2f,%.2f,%.2f]\n",
                USE_2D_HEIGHT_CORR ? "ON" : "OFF", TAG_Z, AZ[0], AZ[1], AZ[2], AZ[3]);
  Serial.printf("bias(m): b1=%.3f b2=%.3f b3=%.3f b4=%.3f\n", bias[0], bias[1], bias[2], bias[3]);
  Serial.printf("timeout: %u ms\n", (unsigned)ANCHOR_TIMEOUT_MS);
}

// =================== LOOP UWB ===================
void loopUWB() {
  DW1000Ranging.loop();
  handleSerial();

  // Calibration window end
  if (cal_active && (millis() - cal_start_ms > CAL_MS)) {
    cal_active = false;

    // 1. คำนวณระยะอ้างอิง 3 มิติ (3D Ref Distance)
    float ex[4];
    for (int i = 0; i < 4; i++) {
      float dx = cal_ref_x - AX[i];
      float dy = cal_ref_y - AY[i];
      float dz = TAG_Z - AZ[i]; 
      ex[i] = sqrtf(dx*dx + dy*dy + dz*dz); 
    }

    // 2. คำนวณ Bias ใหม่
    float newBias[4] = { bias[0], bias[1], bias[2], bias[3] };
    
    Serial.println("--- Calibration Report ---");
    for (int i = 0; i < 4; i++) {
      // Print จำนวน Sample ที่เก็บได้ (เพื่อ Debug)
      Serial.printf("[CAL] Anchor %d: collected %d samples... ", i+1, cal_cnt[i]);
      
      if (cal_cnt[i] > 5) { 
        float avg = (float)(cal_sum[i] / (double)cal_cnt[i]); 
        newBias[i] = avg - ex[i];
        Serial.printf("OK! New Bias = %.3f\n", newBias[i]);
      } else {
        Serial.println("FAIL! (Not enough data)");
      }
    }
    Serial.println("--------------------------");

    if (!cal_multi_point) {
      for (int i = 0; i < 4; i++) bias[i] = newBias[i];
      Serial.printf("[CAL] done ref=(%.2f,%.2f)\n", cal_ref_x, cal_ref_y);
      Serial.println("[CAL] type SAVE or open /save (web) to store.");
    } else {
      for (int i = 0; i < 4; i++) {
        // สะสมค่า Bias เข้าตัวแปร Multi-point (ถ้า Fail จะเป็นค่าเดิม)
        mp_bias_sum[i] += newBias[i];
        mp_bias_cnt[i] += 1;
      }
      Serial.printf("[CALP] point stored ref=(%.2f,%.2f)\n", cal_ref_x, cal_ref_y);
      Serial.println("[CALP] next point -> /calp?x=..&y=.. , finish -> /save");
    }

    // >>> UPDATE STATE: 2 = Success / Finished
    cal_state = 2; 
    Serial.println("[CAL] Finished. State -> 2");
  }

  // Anchor timeout
  uint32_t now = millis();
  for (int i = 0; i < 4; i++) {
    if (validRange(fA[i]) && (now - tA[i] > ANCHOR_TIMEOUT_MS)) {
      fA[i] = -1;
      aStat[i] = -1;
    }
  }

  // Build Meas
  Meas m[4];
  int n = 0;

  for (int i = 0; i < 4; i++) {
    if (!validRange(fA[i])) continue;

    float r3 = fA[i]; 

    // Guard: min
    if (r3 < MIN_RANGE_M) r3 = MIN_RANGE_M;

    float r2;
    if (!flattenRange2D(i, r3, r2)) continue; 

    // Guard: min
    if (r2 < MIN_RANGE_M) r2 = MIN_RANGE_M;

    m[n].ax = AX[i];
    m[n].ay = AY[i];
    m[n].r  = r2;
    m[n].id = i;
    n++;
  }

  // Solve
  float x_raw = -1, y_raw = -1, rmse = 999;
  bool okSolve = false;

  float x0 = have_xy ? x_f : FIELD_W * 0.5f;
  float y0 = have_xy ? y_f : FIELD_H * 0.5f;

  if (n >= 3) {
    okSolve = solveXY_GN(m, n, x_raw, y_raw, rmse, x0, y0);

    // Outlier removal
    if (okSolve && n == 4 && rmse > RMSE_GATE_M) {
      int wi = worstResidualIndex(m, n, x_raw, y_raw);
      if (wi >= 0) {
        Meas m2[3];
        int k = 0;
        for (int i = 0; i < n; i++) if (i != wi) m2[k++] = m[i];

        float x2, y2, rmse2;
        bool ok2 = solveXY_GN(m2, 3, x2, y2, rmse2, x_raw, y_raw);
        if (ok2 && rmse2 < rmse) { x_raw = x2; y_raw = y2; rmse = rmse2; }
      }
    }
  }

  // RMSE gate
  const bool okBasic = (n >= 3) && okSolve;
  const bool acceptStrong = okBasic && (rmse <= RMSE_GATE_M);
  const bool acceptWeak   = okBasic && (rmse <= RMSE_HARD_M);

  last_accept = acceptWeak;
  last_n      = n;
  last_rmse   = okBasic ? rmse : -1;

  if (acceptWeak) {
    // (1) deadband
    const float DEAD_XY = 0.02f;
    if (have_xy) {
      if (fabs(x_raw - x_f) < DEAD_XY) x_raw = x_f;
      if (fabs(y_raw - y_f) < DEAD_XY) y_raw = y_f;
    }

    // (2) max step
    if (have_xy) {
      float dx = x_raw - x_f;
      float dy = y_raw - y_f;
      float step = hypot2(dx, dy);
      if (step > MAX_STEP_M) {
        float s = MAX_STEP_M / step;
        x_raw = x_f + dx * s;
        y_raw = y_f + dy * s;
      }
    }

    // (3) adaptive smoothing
    float alpha = computePosAlpha(n, rmse);
    if (!acceptStrong) alpha = clampf(alpha * 0.55f, XY_ALPHA_MIN, XY_ALPHA_BASE);

    if (!have_xy) {
      x_f = x_raw;
      y_f = y_raw;
      have_xy = true;
    } else {
      x_f = x_f + alpha * (x_raw - x_f);
      y_f = y_f + alpha * (y_raw - y_f);
    }
  }

  // Print
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= PRINT_MS) {
    lastPrint = millis();

    Serial.printf("[TAG1] n=%d  A1=%s  A2=%s  A3=%s  A4=%s | x=%.2f  y=%.2f | rmse=%.3f | st=%d | open=http://%s/\n",
                  n,
                  anchorString(0).c_str(), anchorString(1).c_str(), anchorString(2).c_str(), anchorString(3).c_str(),
                  have_xy ? x_f : -1.0f, have_xy ? y_f : -1.0f,
                  last_rmse,
                  cal_state, // >>> Print state
                  activeIP().toString().c_str());
  }
}