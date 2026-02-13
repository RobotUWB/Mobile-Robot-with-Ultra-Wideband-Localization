import React, { useEffect, useRef, useState } from "react";

/* ================== ICONS ================== */
const clamp = (v, min, max) => Math.max(min, Math.min(max, v));

const Icons = {
  Play: () => (
    <svg
      width="16"
      height="16"
      fill="currentColor"
      viewBox="0 0 24 24"
      aria-hidden="true"
    >
      <path d="M5 3l14 9-14 9V3z" />
    </svg>
  ),
  Pause: () => (
    <svg
      width="16"
      height="16"
      fill="currentColor"
      viewBox="0 0 24 24"
      aria-hidden="true"
    >
      <path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z" />
    </svg>
  ),

  Wifi: () => (
    <svg
      width="16"
      height="16"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      viewBox="0 0 24 24"
      aria-hidden="true"
    >
      <path d="M5 12.55a11 11 0 0114.08 0M1.64 9a15 15 0 0120.72 0M8.27 16a6 6 0 017.46 0M12 20h.01" />
    </svg>
  ),
};

/* ================== GLOBAL STYLES (เรียบ ๆ แบบคนทำ + เอาไปทำ Figma ง่าย) ================== */
const GlobalStyles = () => (
  <style>{`
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;700&display=swap');

    :root{
      /* tokens */
      --bg: #0b1220;
      --surface: #111a2b;
      --surface2: #0f172a;
      --border: rgba(255,255,255,0.10);

      --text: #e5e7eb;
      --muted: #9ca3af;

      --accent: #3b82f6;
      --success: #16a34a;
      --danger: #dc2626;
      --warning: #f59e0b;

      --shadow: 0 10px 22px rgba(0,0,0,0.22);

      --r: 12px;
      --r2: 10px;
      --gap: 16px;
    }

    * { box-sizing: border-box; }

    body{
      margin:0;
      background: var(--bg);
      font-family:'Inter', sans-serif;
      color: var(--text);
      -webkit-font-smoothing: antialiased;
      overflow-x:hidden;
    }

    .mono { font-family:'JetBrains Mono', monospace; }

    /* panel/card */
    .panel{
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: var(--r);
      box-shadow: var(--shadow);
    }

    .panelTitle{
      font-size: 12px;
      font-weight: 700;
      color: var(--muted);
      letter-spacing: .08em;
      margin-bottom: 12px;
      text-transform: uppercase;
    }

    /* buttons */
    .btn{
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      font-weight: 600;
      user-select: none;

      border: 1px solid var(--border);
      background: rgba(255,255,255,0.02);
      color: var(--text);

      transition: background .15s ease, filter .15s ease;
    }
    .btn:hover{ background: rgba(255,255,255,0.05); }
    .btn:active{ filter: brightness(0.95); }
    .btn:disabled{ opacity: .55; cursor: not-allowed; }

    .btnPrimary{ background: rgba(59,130,246,0.95); border-color: rgba(59,130,246,0.40); }
    .btnPrimary:hover{ background: rgba(59,130,246,1); }

    .btnSuccess{ background: rgba(22,163,74,0.95); border-color: rgba(22,163,74,0.40); }
    .btnSuccess:hover{ background: rgba(22,163,74,1); }

    .btnNeutral{ background: rgba(255,255,255,0.05); }
    .btnDanger{ background: rgba(220,38,38,0.95); border-color: rgba(220,38,38,0.35); }
    .btnDanger:hover{ background: rgba(220,38,38,1); }

    .btnGhost{
      background: rgba(255,255,255,0.02);
      border-color: var(--border);
      color: var(--muted);
    }
    .btnGhost:hover{ background: rgba(255,255,255,0.05); color: var(--text); }

    .btnIcon{
      width: 36px;
      height: 36px;
      padding: 0;
      border-radius: 10px;
    }

    /* inputs */
    input.cal-in{
      border: 1px solid var(--border);
      background: rgba(0,0,0,0.18);
      color: var(--text);
      outline: none;
      border-radius: 10px;
      padding: 8px 10px;
      text-align: center;
      font-family: 'JetBrains Mono', monospace;
      font-size: 12px;
      width: 76px;
    }
    input.cal-in:focus{
      border-color: rgba(59,130,246,0.55);
      box-shadow: 0 0 0 3px rgba(59,130,246,0.14);
    }
/* ✅ HIDE number arrows (spinner) */
input.cal-in[type=number]::-webkit-outer-spin-button,
input.cal-in[type=number]::-webkit-inner-spin-button{
  -webkit-appearance: none;
  margin: 0;
}
input.cal-in[type=number]{
  -moz-appearance: textfield; /* Firefox */
  appearance: textfield;
}

    /* slider */
    input[type=range]{
      -webkit-appearance:none;
      width:100%;
      background:transparent;
    }
    input[type=range]::-webkit-slider-thumb{
      -webkit-appearance:none;
      height: 14px; width: 14px;
      border-radius: 50%;
      background: var(--accent);
      cursor:pointer;
      margin-top:-5px;
      box-shadow:none;
    }
    input[type=range]::-webkit-slider-runnable-track{
      width:100%;
      height: 4px;
      cursor:pointer;
      background: rgba(255,255,255,0.12);
      border-radius: 999px;
    }

    /* simple spin (พอมีชีวิต แต่ไม่เว่อร์) */
    @keyframes spin { 100% { transform: rotate(360deg); } }
    .spin { animation: spin 1s linear infinite; }

    /* scrollbar */
    ::-webkit-scrollbar { width: 8px; }
    ::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.16); border-radius: 999px; }
    ::-webkit-scrollbar-thumb:hover { background: rgba(255,255,255,0.22); }
  `}</style>
);
/* ================== CONFIG ================== */
// UWB Tag (192.168.88.99) -> Proxy: /pos
const POS_BASE = "/pos";

// Robot Controller STM32 (192.168.88.115) -> Proxy: /robot
const CMD_BASE = "/robot";

const API_URL = `${POS_BASE}/json`;
const CMD_URL = `${CMD_BASE}/cmd`;
const api = (path) => `${POS_BASE}${path}`;

const FIELD_W = 3000;
const FIELD_H = 2000;

// WebSocket Control (192.168.88.115:81)
const CMD_WS_URL = "ws://192.168.88.115:81";
const WS_MAX_MISSED = 20;

const ZOOM_MIN = 0.05,
  ZOOM_MAX = 0.4,
  ZOOM_STEP = 0.01;

/* ================== MANUAL DRIVE (TEST) ================== */
// ✅ เปลี่ยน cmd ให้ตรงกับ ESP32/STM32 ของคุณได้เลย
// ตัวอย่างถ้าคุณอยากส่งเป็น "W","A","S","D" ก็แก้ value เป็น "W" etc.
const DRIVE_CMDS = {
  W: "FWD",
  A: "LEFT",
  S: "BWD",
  D: "RIGHT",
};
const ROT_CMDS = {
  Q: "ROTL",
  E: "ROTR",
};

// หยุดตอนปล่อยปุ่มเดิน (หรือไม่มีปุ่มค้าง)
const DRIVE_STOP_CMD = "STOP";

// Spacebar behavior:
// - "STOP"  = กดค้างส่ง STOP (ปล่อยแล้วถ้ายังค้าง WASD จะกลับไปส่งเดินต่อ)
// - "PAUSE" = กดค้างส่ง PAUSE / ปล่อยส่ง RESUME (แนะนำถ้า STOP เป็น Emergency จริง)
const SPACE_MODE = "STOP";

/* ================== APP COMPONENT ================== */
export default function App() {
  // ===== Calibration UI =====
  const [refX, setRefX] = useState("");
  const [refY, setRefY] = useState("");
  const [calState, setCalState] = useState(0); // 0..4 from ESP32 json.cs

  const CAL_TEXT = [
    "READY",
    "CALIBRATING...",
    "SUCCESS!",
    "FAILED",
    "RESET DONE",
  ];
  const CAL_COLOR = [
    "var(--muted)",
    "var(--warning)",
    "var(--success)",
    "var(--danger)",
    "var(--accent)",
  ];

  /* --- State: Canvas & Data --- */
  const canvasRef = useRef(null);
  const [scale, setScale] = useState(0.15);

  const [anchors, setAnchors] = useState([
    { id: "A1", x_mm: 0, y_mm: 0 },
    { id: "A2", x_mm: 0, y_mm: 2000 },
    { id: "A3", x_mm: 3000, y_mm: 0 },
    { id: "A4", x_mm: 3000, y_mm: 2000 },
  ]);

  const [ranges, setRanges] = useState({ A1: 0, A2: 0, A3: 0, A4: 0 });

  // WebSocket for Control
  const wsCmdRef = useRef(null);
  const [wsConnected, setWsConnected] = useState(false);

  // Tag1
  const [pose, setPose] = useState({ x_mm: 0, y_mm: 0, yaw: 0 });
  const [yawOffset, setYawOffset] = useState(() => {
    return parseFloat(localStorage.getItem("uwb_yaw_offset") || "0");
  });
  // RMSE (ถ้ามีใน json)
  const [rmse, setRmse] = useState(null);

  /* --- State: System --- */
  const [connected, setConnected] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [showTags, setShowTags] = useState(true);
  const [toast, setToast] = useState({ show: false, msg: "", type: "info" });

  /* --- Refs for Animation Loop --- */
  const scaleRef = useRef(scale);
  const poseRef = useRef(pose);
  const anchorsRef = useRef(anchors);
  const isFetchingRef = useRef(false);
  const missedHeartbeatsRef = useRef(0); // ✅ Heartbeat Counter
  const showTagsRef = useRef(showTags);
  const rangesRef = useRef(ranges);
  const yawOffsetRef = useRef(0);
  const mouseRef = useRef({ x: 0, y: 0, active: false }); // ✅ Mouse tracking

  useEffect(() => {
    scaleRef.current = scale;
  }, [scale]);
  useEffect(() => {
    anchorsRef.current = anchors;
  }, [anchors]);
  useEffect(() => {
    showTagsRef.current = showTags;
  }, [showTags]);
  useEffect(() => {
    rangesRef.current = ranges;
  }, [ranges]);
  useEffect(() => {
    poseRef.current = pose;
  }, [pose]);
  useEffect(() => {
    yawOffsetRef.current = yawOffset;
    localStorage.setItem("uwb_yaw_offset", yawOffset.toString());
  }, [yawOffset]);

  /* --- Helper: Toast --- */
  const showToast = (msg, type = "info") => {
    setToast({ show: true, msg, type });
    setTimeout(() => setToast((prev) => ({ ...prev, show: false })), 2600);
  };

  /* --- API Actions --- */
  const sendCmd = async ({ cmd }) => {
    try {
      const res = await fetch(CMD_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ cmd }), // ✅ ESP32 ต้องการ {cmd:"FWD"} แบบนี้
        cache: "no-store",
      });

      const txt = await res.text().catch(() => "");
      if (!res.ok) console.error("CMD Error", res.status, txt);
      return res.ok;
    } catch (e) {
      console.error("CMD fetch failed:", e);
      return false;
    }
  };

  // ===== Calibration Actions (GET endpoints like friend's ESP32) =====
  const calT1 = async () => {
    const x = parseFloat(refX);
    const y = parseFloat(refY);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      showToast("REF must be numbers", "error");
      return;
    }

    try {
      const url = api(
        `/cal?x=${encodeURIComponent(x)}&y=${encodeURIComponent(y)}`,
      );
      const r = await fetch(url);
      if (!r.ok) throw new Error("CAL failed");
      showToast("CAL T1 started", "success");
    } catch (e) {
      console.error(e);
      showToast("CAL T1 failed (check /cal endpoint)", "error");
    }
  };

  const saveT1 = async () => {
    try {
      const r = await fetch(api("/save"));
      if (!r.ok) throw new Error("SAVE failed");
      showToast("SAVE T1 OK", "success");
    } catch (e) {
      console.error(e);
      showToast("SAVE T1 failed (check /save endpoint)", "error");
    }
  };

  const resetT1 = async () => {
    try {
      const r = await fetch(api("/reset"));
      if (!r.ok) throw new Error("RESET failed");
      showToast("RESET T1 OK", "success");
    } catch (e) {
      console.error(e);
      showToast("RESET T1 failed (check /reset endpoint)", "error");
    }
  };

  const calDirection = () => {
    // เซ็ตให้มุมปัจจุบันของหุ่น กลายเป็น 0 (หันซ้าย)
    const currentYaw = poseRef.current.yaw || 0;
    setYawOffset(currentYaw);
    showToast("CAL DIRECTION OK", "success");
  };

  /* ================== MANUAL DRIVE LOGIC ================== */
  const [driveKey, setDriveKey] = useState(null); // "W"/"A"/"S"/"D"/null
  const [rotKey, setRotKey] = useState(null); // "Q" | "E" | null

  // Refs for interval access
  const driveKeyRef = useRef(null);
  const rotKeyRef = useRef(null);

  useEffect(() => {
    driveKeyRef.current = driveKey;
  }, [driveKey]);
  useEffect(() => {
    rotKeyRef.current = rotKey;
  }, [rotKey]);

  const pressedOrderRef = useRef([]); // เก็บลำดับปุ่มที่ค้าง (อันล่าสุดมี priority)
  const [stopHeld, setStopHeld] = useState(false);
  const stopHeldRef = useRef(false);

  const connectedRef2 = useRef(connected);
  const isPausedRef2 = useRef(isPaused);

  useEffect(() => {
    connectedRef2.current = connected;
  }, [connected]);

  useEffect(() => {
    isPausedRef2.current = isPaused;
  }, [isPaused]);

  const isTypingTarget = (el) => {
    if (!el) return false;
    const tag = (el.tagName || "").toUpperCase();
    return tag === "INPUT" || tag === "TEXTAREA" || el.isContentEditable;
  };

  const normalizeMoveKey = (k) => {
    const up = String(k || "").toUpperCase();
    return ["W", "A", "S", "D"].includes(up) ? up : null;
  };

  const computeActiveMoveKey = () => {
    const arr = pressedOrderRef.current;
    return arr.length ? arr[arr.length - 1] : null;
  };
  // Function to send drive command (WebSocket Preferred, fallback to HTTP)
  const sendDriveCmd = async (cmdVal) => {
    // 1. WebSocket (Text Mode)
    if (wsCmdRef.current && wsCmdRef.current.readyState === WebSocket.OPEN) {
      wsCmdRef.current.send(cmdVal);
      return;
    }

    // 2. HTTP Fallback
    try {
      if (cmdVal === "STOP") {
        await fetch(`${CMD_BASE}/action?type=STOP`);
      } else {
        await fetch(CMD_URL, {
          method: "POST", // ESP32 might still expect POST/GET, but API says Text via WS
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `cmd=${cmdVal}`,
        });
      }
    } catch (err) {
      console.error("Cmd Error:", err);
      console.error("Cmd Error:", err);
    }
  };

  // ✅ New: Send command every 200ms if keys are held
  useEffect(() => {
    const intervalId = setInterval(() => {
      if (isPausedRef2.current) return;

      // Priority 1: STOP held
      if (stopHeldRef.current) {
        sendDriveCmd(DRIVE_STOP_CMD);
        return;
      }

      // Priority 2: Rotation Key
      const rKey = rotKeyRef.current;
      if (rKey) {
        sendDriveCmd(ROT_CMDS[rKey]);
        return;
      }

      // Priority 3: Drive Key
      const dKey = driveKeyRef.current;
      if (dKey) {
        sendDriveCmd(DRIVE_CMDS[dKey]);
      }
    }, 200);

    return () => clearInterval(intervalId);
  }, []);

  const applyManualDrive = async () => {
    // ถ้าถูก Pause อยู่ (จาก UI) ไม่สั่งเดินซ้ำ
    if (isPausedRef2.current) return;

    // ✅ ถ้ากำลังกด STOP ค้างอยู่ ให้สั่ง STOP ตลอด และไม่ล้างปุ่มที่ค้าง
    if (stopHeldRef.current) {
      sendDriveCmd(DRIVE_STOP_CMD);
      return;
    }

    const k = computeActiveMoveKey();
    setDriveKey(k);

    if (k) {
      sendDriveCmd(DRIVE_CMDS[k]);
    } else {
      sendDriveCmd(DRIVE_STOP_CMD);
    }
  };

  const rotatePress = async (k) => {
    if (isPausedRef2.current) return;
    setRotKey(k);
    await sendDriveCmd(ROT_CMDS[k]);
  };

  const rotateRelease = async () => {
    setRotKey(null);
    await sendDriveCmd(DRIVE_STOP_CMD); // ปล่อยแล้วหยุดหมุน
  };

  const manualPress = async (kRaw) => {
    const k = normalizeMoveKey(kRaw);
    if (!k) return;

    const arr = pressedOrderRef.current;
    if (!arr.includes(k)) arr.push(k);
    await applyManualDrive();
  };

  const manualRelease = async (kRaw) => {
    const k = normalizeMoveKey(kRaw);
    if (!k) return;

    pressedOrderRef.current = pressedOrderRef.current.filter((x) => x !== k);
    await applyManualDrive();
  };
  const manualReleaseAll = async () => {
    pressedOrderRef.current = [];
    setDriveKey(null);

    // ✅ เคลียร์ stop hold ด้วย
    stopHeldRef.current = false;
    setStopHeld(false);

    await sendCmd({ cmd: DRIVE_STOP_CMD });
  };

  const stopHoldPress = () => {
    // ✅ ไม่ล้าง pressedOrderRef เพื่อให้ปล่อย STOP แล้วกลับไปวิ่งต่อ
    stopHeldRef.current = true;
    setStopHeld(true);
    sendCmd({ cmd: DRIVE_STOP_CMD });
  };

  const stopHoldRelease = async () => {
    stopHeldRef.current = false;
    setStopHeld(false);
    await applyManualDrive(); // ✅ ปล่อย STOP แล้วกลับไปตามปุ่มที่ค้างอยู่
  };

  const stopNow = async () => {
    // หยุดแบบ “ตัดคำสั่งเดิน” ไม่ให้กลับไปวิ่งเอง
    pressedOrderRef.current = [];
    setDriveKey(null);

    await sendCmd({ cmd: "STOP" }); // ✅ ส่ง {"cmd":"STOP"}
    // จะโชว์ toast ก็ได้
    // showToast("STOP", "success");
  };
  const codeToRotateKey = (code) => {
    if (code === "KeyQ") return "Q";
    if (code === "KeyE") return "E";
    return null;
  };

  useEffect(() => {
    const codeToMoveKey = (code) => {
      if (code === "KeyW") return "W";
      if (code === "KeyA") return "A";
      if (code === "KeyS") return "S";
      if (code === "KeyD") return "D";
      return null;
    };

    const onKeyDown = (e) => {
      if (isTypingTarget(e.target)) return;

      if (e.code === "Space") {
        if (e.repeat) return;
        e.preventDefault();
        stopHoldPress();
        return;
      }

      const r = codeToRotateKey(e.code);
      if (r) {
        if (e.repeat) return; // Prevent native repeat, use interval instead
        e.preventDefault();
        rotatePress(r);
        return;
      }

      const k = codeToMoveKey(e.code);
      if (k) {
        if (e.repeat) return; // Prevent native repeat, use interval instead
        e.preventDefault();
        manualPress(k);
      }
    };

    const onKeyUp = (e) => {
      if (isTypingTarget(e.target)) return;

      if (e.code === "Space") {
        e.preventDefault();
        stopHoldRelease();
        return;
      }

      const r = codeToRotateKey(e.code);
      if (r) {
        e.preventDefault();
        rotateRelease();
        return;
      }

      const k = codeToMoveKey(e.code);
      if (k) {
        e.preventDefault();
        manualRelease(k);
      }
    };

    const onBlur = () => {
      manualReleaseAll();
    };

    window.addEventListener("keydown", onKeyDown, { passive: false });
    window.addEventListener("keyup", onKeyUp, { passive: false });
    window.addEventListener("blur", onBlur);

    return () => {
      window.removeEventListener("keydown", onKeyDown);
      window.removeEventListener("keyup", onKeyUp);
      window.removeEventListener("blur", onBlur);
    };
  }, []);

  /* --- HTTP Polling for UWB Data + Heading --- */
  useEffect(() => {
    const pollData = async () => {
      try {
        // --- Fetch UWB data from ESP32 (192.168.88.99) ---
        const res = await fetch(API_URL);
        if (!res.ok) throw new Error(`UWB fetch failed: ${res.status}`);
        const j = await res.json();

        missedHeartbeatsRef.current = 0;
        setConnected(true);

        // Position
        const xVal = Number(j.x);
        const yVal = Number(j.y);
        const tag1Ok =
          Number.isFinite(xVal) &&
          Number.isFinite(yVal) &&
          xVal !== -1 &&
          yVal !== -1;

        if (tag1Ok) {
          const xs = anchorsRef.current.map((a) => a.x_mm);
          const ys = anchorsRef.current.map((a) => a.y_mm);
          const minX = Math.min(...xs);
          const maxX = Math.max(...xs);
          const minY = Math.min(...ys);
          const maxY = Math.max(...ys);

          setPose((prev) => ({
            x_mm: clamp(xVal * 1000, minX, maxX),
            y_mm: clamp(yVal * 1000, minY, maxY),
            yaw: prev.yaw,
          }));
        }

        // Anchor distances
        if (j.a && Array.isArray(j.a)) {
          const newRanges = {
            A1: parseFloat(j.a[0]) || 0,
            A2: parseFloat(j.a[1]) || 0,
            A3: parseFloat(j.a[2]) || 0,
            A4: parseFloat(j.a[3]) || 0,
          };
          rangesRef.current = newRanges;
          setRanges(newRanges);
        }

        // Calibration state
        const cs = Number(j.cs);
        if (Number.isFinite(cs)) setCalState(Math.max(0, Math.min(4, cs)));

        // RMSE
        const r = Number(j.rmse);
        if (Number.isFinite(r)) setRmse(r);

        // Anchor positions (anch_xy) from ESP32
        if (j.anch_xy && Array.isArray(j.anch_xy)) {
          const newAnchors = j.anch_xy.map((p, i) => ({
            id: `A${i + 1}`,
            x_mm: p[0] * 1000,
            y_mm: p[1] * 1000,
          }));
          if (newAnchors.length >= 3) setAnchors(newAnchors);
        }
      } catch (err) {
        missedHeartbeatsRef.current += 1;
        if (missedHeartbeatsRef.current >= 10) setConnected(false);
      }
    };

    // Poll UWB data every 200ms
    const dataInterval = setInterval(pollData, 200);
    pollData();

    return () => {
      clearInterval(dataInterval);
    };
  }, []);

  /* --- WebSocket for Robot Control (Heading + Commands) --- */
  useEffect(() => {
    let reconnectTimeout = null;

    const connectWsCmd = () => {
      if (wsCmdRef.current && wsCmdRef.current.readyState === WebSocket.OPEN)
        return;

      console.log("WS-CMD: Connecting to", CMD_WS_URL);
      const ws = new WebSocket(CMD_WS_URL);
      wsCmdRef.current = ws;

      // Heartbeat Interval (PING)
      const pingInterval = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
          ws.send("PING");
        }
      }, 100);

      ws.onopen = () => {
        console.log("WS-CMD: Connected");
        setWsConnected(true);
      };

      ws.onmessage = (event) => {
        try {
          // Ignore PONG for now (or use it to measure latency)
          if (event.data === "PONG") return;

          const j = JSON.parse(event.data);
          // Expect format: {"angle": 45.5}
          const angle = Number(j.angle);
          if (Number.isFinite(angle)) {
            setPose((prev) => ({ ...prev, yaw: angle }));
          }
        } catch (e) {
          console.error("WS-CMD parse error:", e);
        }
      };

      ws.onclose = () => {
        console.log("WS-CMD: Closed, reconnecting...");
        setWsConnected(false);
        wsCmdRef.current = null;
        clearInterval(pingInterval); // Stop pinging
        reconnectTimeout = setTimeout(connectWsCmd, 2000);
      };

      ws.onerror = (e) => {
        console.error("WS-CMD Error:", e);
        ws.close();
      };
    };

    connectWsCmd();

    return () => {
      if (reconnectTimeout) clearTimeout(reconnectTimeout);
      if (wsCmdRef.current) wsCmdRef.current.close();
    };
  }, []);

  /* --- Canvas Drawing (ลด glow/ความ neon ให้ดูคนทำ) --- */
  useEffect(() => {
    const cvs = canvasRef.current;
    if (!cvs) return;
    const ctx = cvs.getContext("2d");
    let raf;

    const COLORS = {
      bg: "#0f172a",
      grid: "rgba(255,255,255,0.05)",
      boundary: "rgba(59,130,246,0.35)",
      anchorOn: "#16a34a",
      anchorOff: "#64748b",
      tag1: "#3b82f6",

      labelBg: "rgba(17,26,43,0.92)",
      labelBd: "rgba(255,255,255,0.10)",
      text: "#e5e7eb",
      muted: "#9ca3af",
    };

    const mmToPx = (mm, s) => mm * s;

    const getBoundsFromAnchors = (anchorsList) => {
      if (anchorsList && anchorsList.length) {
        let minX = Infinity,
          maxX = -Infinity,
          minY = Infinity,
          maxY = -Infinity;

        for (const a of anchorsList) {
          if (typeof a.x_mm !== "number" || typeof a.y_mm !== "number")
            continue;
          minX = Math.min(minX, a.x_mm);
          maxX = Math.max(maxX, a.x_mm);
          minY = Math.min(minY, a.y_mm);
          maxY = Math.max(maxY, a.y_mm);
        }

        if (
          isFinite(minX) &&
          isFinite(maxX) &&
          isFinite(minY) &&
          isFinite(maxY) &&
          minX !== maxX &&
          minY !== maxY
        ) {
          return { minX, maxX, minY, maxY };
        }
      }
      return { minX: 0, maxX: FIELD_W, minY: 0, maxY: FIELD_H };
    };

    const toPx = (x_mm, y_mm, cx, cy, s, bounds) => {
      const midX = (bounds.minX + bounds.maxX) / 2;
      const midY = (bounds.minY + bounds.maxY) / 2;
      return {
        px: cx + mmToPx(x_mm - midX, s),
        py: cy + mmToPx(midY - y_mm, s),
      };
    };
    const drawRobot = (x, y, size, color, yawDeg) => {
      ctx.save();
      ctx.translate(x, y);
      // ชดเชยให้ 0 องศาหันไปทาง "ซ้าย" (Front) และทวนเข็มตามค่า IMU
      const angleRad = ((180 - yawDeg) * Math.PI) / 180;
      ctx.rotate(angleRad);

      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.moveTo(size * 1.5, 0);
      ctx.lineTo(-size, size);
      ctx.lineTo(-size, -size);
      ctx.closePath();
      ctx.fill();

      ctx.strokeStyle = "rgba(255,255,255,0.9)";
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.restore();
    };
    const drawTriangle = (x, y, size, color, dir = "left") => {
      ctx.save();
      ctx.fillStyle = color;
      ctx.beginPath();

      if (dir === "left") {
        // tip -> left
        ctx.moveTo(x - size, y);
        ctx.lineTo(x + size * 0.7, y - size * 0.85);
        ctx.lineTo(x + size * 0.7, y + size * 0.85);
      } else if (dir === "right") {
        ctx.moveTo(x + size, y);
        ctx.lineTo(x - size * 0.7, y - size * 0.85);
        ctx.lineTo(x - size * 0.7, y + size * 0.85);
      } else if (dir === "up") {
        ctx.moveTo(x, y - size);
        ctx.lineTo(x + size * 0.85, y + size * 0.7);
        ctx.lineTo(x - size * 0.85, y + size * 0.7);
      } else {
        // down
        ctx.moveTo(x, y + size);
        ctx.lineTo(x + size * 0.85, y - size * 0.7);
        ctx.lineTo(x - size * 0.85, y - size * 0.7);
      }

      ctx.closePath();
      ctx.fill();

      ctx.strokeStyle = "rgba(255,255,255,0.18)";
      ctx.lineWidth = 1.2;
      ctx.stroke();
      ctx.restore();
    };

    const drawTagLabel = (text, x, y, above = true) => {
      ctx.save();
      ctx.font = "600 12px Inter";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";

      const padX = 10;
      const h = 22;
      const w = ctx.measureText(text).width + padX * 2;
      const bx = x - w / 2;
      const by = above ? y - 26 - h : y + 26;

      ctx.fillStyle = COLORS.labelBg;
      ctx.strokeStyle = COLORS.labelBd;
      ctx.lineWidth = 1;

      ctx.beginPath();
      ctx.roundRect(bx, by, w, h, 8);
      ctx.fill();
      ctx.stroke();

      ctx.fillStyle = COLORS.text;
      ctx.fillText(text, x, by + h / 2);
      ctx.restore();
    };

    const draw = () => {
      const W = cvs.width,
        H = cvs.height;
      const cx = W / 2,
        cy = H / 2;
      const s = scaleRef.current;
      const show = showTagsRef.current;
      const currentRanges = rangesRef.current;
      const anchorsNow = anchorsRef.current || [];
      const bounds = getBoundsFromAnchors(anchorsNow);

      // Clear
      ctx.fillStyle = COLORS.bg;
      ctx.fillRect(0, 0, W, H);

      // Grid
      ctx.strokeStyle = COLORS.grid;
      ctx.lineWidth = 1;
      ctx.beginPath();
      const STEP = 500;

      const xStart = Math.floor(bounds.minX / STEP) * STEP;
      const xEnd = Math.ceil(bounds.maxX / STEP) * STEP;
      for (let x = xStart; x <= xEnd; x += STEP) {
        let p1 = toPx(x, bounds.minY, cx, cy, s, bounds);
        let p2 = toPx(x, bounds.maxY, cx, cy, s, bounds);
        ctx.moveTo(p1.px, p1.py);
        ctx.lineTo(p2.px, p2.py);
      }

      const yStart = Math.floor(bounds.minY / STEP) * STEP;
      const yEnd = Math.ceil(bounds.maxY / STEP) * STEP;
      for (let y = yStart; y <= yEnd; y += STEP) {
        let p1 = toPx(bounds.minX, y, cx, cy, s, bounds);
        let p2 = toPx(bounds.maxX, y, cx, cy, s, bounds);
        ctx.moveTo(p1.px, p1.py);
        ctx.lineTo(p2.px, p2.py);
      }
      ctx.stroke();

      // Boundary
      const b1 = toPx(bounds.minX, bounds.minY, cx, cy, s, bounds);
      const b2 = toPx(bounds.maxX, bounds.minY, cx, cy, s, bounds);
      const b3 = toPx(bounds.maxX, bounds.maxY, cx, cy, s, bounds);
      const b4 = toPx(bounds.minX, bounds.maxY, cx, cy, s, bounds);

      ctx.strokeStyle = COLORS.boundary;
      ctx.setLineDash([8, 8]);
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(b1.px, b1.py);
      ctx.lineTo(b2.px, b2.py);
      ctx.lineTo(b3.px, b3.py);
      ctx.lineTo(b4.px, b4.py);
      ctx.closePath();
      ctx.stroke();
      ctx.setLineDash([]);

      // Anchors
      for (const a of anchorsRef.current) {
        const { px, py } = toPx(a.x_mm, a.y_mm, cx, cy, s, bounds);

        const rangeVal = currentRanges[a.id] || 0;
        const isOnline = rangeVal > 0;

        ctx.fillStyle = isOnline ? COLORS.anchorOn : COLORS.anchorOff;
        ctx.beginPath();
        ctx.arc(px, py, isOnline ? 5 : 4, 0, Math.PI * 2);
        ctx.fill();

        if (show) {
          ctx.strokeStyle = isOnline
            ? "rgba(22,163,74,0.30)"
            : "rgba(148,163,184,0.20)";
          ctx.lineWidth = 1;

          ctx.beginPath();
          ctx.arc(px, py, 14, 0, Math.PI * 2);
          ctx.stroke();

          ctx.fillStyle = isOnline ? COLORS.text : COLORS.muted;
          ctx.font = "600 13px Inter";
          ctx.textAlign = "center";
          ctx.textBaseline = "alphabetic";

          const labelText = `${a.id} ${rangeVal.toFixed(2)}m`;
          ctx.fillText(labelText, px, py - 18);
        }
      }

      // Tag1 (Only)
      const t1 = poseRef.current;

      const t1Finite = Number.isFinite(t1.x_mm) && Number.isFinite(t1.y_mm);
      if (t1Finite) {
        const p1 = toPx(t1.x_mm, t1.y_mm, cx, cy, s, bounds);

        // วาดหุ่นแบบหมุนได้ (0 องศา = หันซ้าย)
        drawRobot(
          p1.px,
          p1.py,
          10,
          COLORS.tag1,
          (t1.yaw || 0) - yawOffsetRef.current,
        );
        if (show) drawTagLabel("Robot UWB", p1.px, p1.py, true);

        // XY label (Tag1)
        if (show) {
          ctx.save();
          ctx.font = "11px JetBrains Mono";
          ctx.textAlign = "center";
          ctx.textBaseline = "alphabetic";

          const label = `X:${(t1.x_mm / 1000).toFixed(2)} Y:${(t1.y_mm / 1000).toFixed(2)}`;
          const tw = ctx.measureText(label).width + 16;

          ctx.fillStyle = COLORS.labelBg;
          ctx.strokeStyle = COLORS.labelBd;
          ctx.beginPath();
          ctx.roundRect(p1.px - tw / 2, p1.py + 24, tw, 24, 6);
          ctx.fill();
          ctx.stroke();

          ctx.fillStyle = COLORS.text;
          ctx.fillText(label, p1.px, p1.py + 42);
          ctx.restore();
        }
      }

      // ✅ Mouse Hover Tooltip
      if (mouseRef.current.active) {
        const mx = mouseRef.current.x;
        const my = mouseRef.current.y;

        // Inverse Calcs
        const midX = (bounds.minX + bounds.maxX) / 2;
        const midY = (bounds.minY + bounds.maxY) / 2;

        const mmX = (mx - cx) / s + midX;
        const mmY = midY - (my - cy) / s;

        const txt = `X:${(mmX / 1000).toFixed(2)} Y:${(mmY / 1000).toFixed(2)}`;

        ctx.save();
        ctx.font = "600 12px Inter";
        ctx.textAlign = "left";
        ctx.textBaseline = "bottom";

        // Draw crosshair
        ctx.strokeStyle = "rgba(255,255,255,0.3)";
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(mx, 0);
        ctx.lineTo(mx, H);
        ctx.moveTo(0, my);
        ctx.lineTo(W, my);
        ctx.stroke();
        ctx.setLineDash([]);

        // Tooltip
        const pad = 6;
        const tw = ctx.measureText(txt).width + pad * 2;
        const th = 20;
        const tx = mx + 10;
        const ty = my - 10;

        ctx.fillStyle = "rgba(0,0,0,0.8)";
        ctx.strokeStyle = "rgba(255,255,255,0.2)";
        ctx.beginPath();
        ctx.roundRect(tx, ty - th, tw, th, 4);
        ctx.fill();
        ctx.stroke();

        ctx.fillStyle = "#fff";
        ctx.fillText(txt, tx + pad, ty - pad + 2);

        ctx.restore();
      }

      raf = requestAnimationFrame(draw);
    };

    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, []);

  /* ================== RENDER ================== */
  const toastBg =
    toast.type === "error"
      ? "rgba(220,38,38,0.95)"
      : toast.type === "success"
        ? "rgba(22,163,74,0.95)"
        : "rgba(59,130,246,0.95)";

  return (
    <>
      <GlobalStyles />

      {/* Toast (เรียบ ไม่เด้งแรง) */}
      <div
        style={{
          position: "fixed",
          top: 18,
          right: 18,
          zIndex: 99,
          background: toastBg,
          color: "#fff",
          padding: "10px 14px",
          borderRadius: 10,
          border: "1px solid rgba(255,255,255,0.12)",
          boxShadow: "0 10px 22px rgba(0,0,0,0.22)",
          transform: toast.show ? "translateY(0)" : "translateY(-8px)",
          opacity: toast.show ? 1 : 0,
          transition: "opacity .18s ease, transform .18s ease",
          fontWeight: 600,
          fontSize: 13,
          pointerEvents: "none",
        }}
      >
        {toast.msg}
      </div>

      <div
        style={{
          minHeight: "100vh",
          display: "flex",
          flexDirection: "column",
          maxWidth: 1400,
          margin: "0 auto",
          padding: 20,
          gap: 16,
        }}
      >
        {/* Header */}
        <header
          className="panel"
          style={{
            padding: "16px 18px",
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
            gap: 16,
          }}
        >
          <div style={{ display: "flex", gap: 12, alignItems: "center" }}>
            <div
              style={{
                width: 40,
                height: 40,
                background: "rgba(255,255,255,0.04)",
                border: "1px solid rgba(255,255,255,0.10)",
                borderRadius: 10,
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
                color: "var(--muted)",
              }}
              title="ESP32: Dual Setup (Pos=.53, Cmd=.115)"
            >
              <Icons.Wifi />
            </div>

            <div>
              <h1
                style={{
                  margin: 0,
                  fontSize: 18,
                  fontWeight: 700,
                  letterSpacing: 0.2,
                }}
              >
                UWB Localization
              </h1>

              <div
                style={{
                  display: "flex",
                  alignItems: "center",
                  gap: 8,
                  marginTop: 4,
                }}
              >
                <div
                  style={{
                    width: 8,
                    height: 8,
                    borderRadius: "50%",
                    background: connected ? "var(--success)" : "var(--danger)",
                  }}
                />
                <span
                  style={{
                    fontSize: 13,
                    color: connected ? "var(--success)" : "var(--danger)",
                    fontWeight: 700,
                  }}
                >
                  {connected ? "ONLINE" : "DISCONNECTED"}
                </span>
              </div>
            </div>
          </div>

          <div style={{ display: "flex", gap: 22, alignItems: "center" }}>
            <Metric
              label="COORD X"
              value={(pose.x_mm / 1000).toFixed(2)}
              unit="m"
            />
            <Divider />
            <Metric
              label="COORD Y"
              value={(pose.y_mm / 1000).toFixed(2)}
              unit="m"
            />
            <Divider />
            <Metric
              label="HEADING"
              value={((pose.yaw || 0) - yawOffset).toFixed(1)}
              unit="°"
            />
            <Divider />
            <Metric
              label="RMSE"
              value={rmse === null ? "0.00" : rmse.toFixed(3)}
              unit="m"
            />
          </div>
        </header>

        {/* Main Grid */}
        <div
          style={{
            display: "grid",
            gridTemplateColumns: "300px 1fr",
            gap: 16,
            flex: 1,
          }}
        >
          {/* Left Sidebar */}
          <div style={{ display: "flex", flexDirection: "column", gap: 16 }}>
            {/* Anchors + Calibration */}
            <div className="panel" style={{ padding: 16 }}>
              <div className="panelTitle">Anchor distances</div>

              <div style={{ display: "grid", gap: 10 }}>
                {Object.entries(ranges).map(([k, v]) => (
                  <div
                    key={k}
                    style={{
                      display: "flex",
                      justifyContent: "space-between",
                      alignItems: "center",
                    }}
                  >
                    <span
                      style={{
                        color: "#ffffff",
                        fontSize: 14,
                        fontWeight: 700,
                      }}
                    >
                      {k}
                    </span>
                    <span
                      className="mono"
                      style={{ fontSize: 13, color: "#ffffff" }}
                    >
                      {v.toFixed(2)}m
                    </span>
                  </div>
                ))}
              </div>

              <div
                style={{
                  marginTop: 14,
                  paddingTop: 14,
                  borderTop: "1px solid var(--border)",
                }}
              >
                <div
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    marginBottom: 10,
                  }}
                >
                  <div className="panelTitle" style={{ margin: 0 }}>
                    Tag1 calibration
                  </div>
                  <div
                    style={{
                      fontSize: 12,
                      fontWeight: 800,
                      color: CAL_COLOR[calState] || "var(--muted)",
                    }}
                  >
                    {CAL_TEXT[calState] || "READY"}
                  </div>
                </div>

                <div style={{ display: "grid", gap: 10 }}>
                  <div
                    style={{
                      display: "flex",
                      alignItems: "center",
                      gap: 10,
                      flexWrap: "wrap",
                    }}
                  >
                    <span
                      style={{
                        fontSize: 12,
                        color: "var(--muted)",
                        fontWeight: 800,
                      }}
                    >
                      REF
                    </span>

                    <input
                      className="cal-in"
                      type="number"
                      step="0.01"
                      value={refX}
                      onChange={(e) => setRefX(e.target.value)}
                      inputMode="decimal"
                      onFocus={(e) => e.target.select()}
                    />

                    <input
                      className="cal-in"
                      type="number"
                      step="0.01"
                      value={refY}
                      onChange={(e) => setRefY(e.target.value)}
                      inputMode="decimal"
                      onFocus={(e) => e.target.select()}
                    />
                  </div>

                  <div
                    style={{
                      display: "flex",
                      alignItems: "center",
                      gap: 10,
                      flexWrap: "wrap",
                    }}
                  >
                    <button
                      onClick={calT1}
                      disabled={!connected}
                      className="btn btnPrimary"
                      style={{
                        height: 34,
                        padding: "0 12px",
                        borderRadius: 10,
                      }}
                    >
                      CAL
                    </button>

                    <button
                      onClick={calDirection}
                      disabled={!connected}
                      className="btn btnNeutral"
                      style={{
                        height: 34,
                        padding: "0 12px",
                        borderRadius: 10,
                      }}
                      title="Set Current Direction as 0 (Front)"
                    >
                      CAL DIR
                    </button>

                    <button
                      onClick={saveT1}
                      disabled={!connected}
                      className="btn btnSuccess"
                      style={{
                        height: 34,
                        padding: "0 12px",
                        borderRadius: 10,
                      }}
                    >
                      SAVE
                    </button>

                    <button
                      onClick={resetT1}
                      disabled={!connected}
                      className="btn btnNeutral"
                      style={{
                        height: 34,
                        padding: "0 12px",
                        borderRadius: 10,
                      }}
                    >
                      RESET
                    </button>
                  </div>
                </div>

                <div
                  style={{
                    marginTop: 10,
                    fontSize: 12,
                    color: "var(--muted)",
                    lineHeight: 1.35,
                  }}
                />
              </div>
            </div>

            {/* Manual Drive (TEST) */}
            <div className="panel" style={{ padding: 16 }}>
              <div className="panelTitle">Manual drive (test)</div>

              <div
                style={{
                  display: "grid",
                  gridTemplateColumns: "repeat(3, 60px)",
                  gap: 8,
                  justifyContent: "center",
                }}
              >
                {/* Q */}
                <button
                  className={`btn ${rotKey === "Q" ? "btnPrimary" : "btnNeutral"}`}
                  onMouseDown={() => rotatePress("Q")}
                  onMouseUp={rotateRelease}
                  onMouseLeave={rotateRelease}
                >
                  Q
                </button>

                {/* W */}
                <button
                  className={`btn ${driveKey === "W" ? "btnPrimary" : "btnNeutral"}`}
                  onMouseDown={() => manualPress("W")}
                  onMouseUp={() => manualRelease("W")}
                  onMouseLeave={() => manualRelease("W")}
                >
                  W
                </button>

                {/* E */}
                <button
                  className={`btn ${rotKey === "E" ? "btnPrimary" : "btnNeutral"}`}
                  onMouseDown={() => rotatePress("E")}
                  onMouseUp={rotateRelease}
                  onMouseLeave={rotateRelease}
                >
                  E
                </button>

                {/* A */}
                <button
                  className={`btn ${driveKey === "A" ? "btnPrimary" : "btnNeutral"}`}
                  onMouseDown={() => manualPress("A")}
                  onMouseUp={() => manualRelease("A")}
                  onMouseLeave={() => manualRelease("A")}
                >
                  A
                </button>

                {/* S */}
                <button
                  className={`btn ${driveKey === "S" ? "btnPrimary" : "btnNeutral"}`}
                  onMouseDown={() => manualPress("S")}
                  onMouseUp={() => manualRelease("S")}
                  onMouseLeave={() => manualRelease("S")}
                >
                  S
                </button>

                {/* D */}
                <button
                  className={`btn ${driveKey === "D" ? "btnPrimary" : "btnNeutral"}`}
                  onMouseDown={() => manualPress("D")}
                  onMouseUp={() => manualRelease("D")}
                  onMouseLeave={() => manualRelease("D")}
                >
                  D
                </button>
              </div>

              <div
                style={{
                  marginTop: 12,
                  display: "flex",
                  gap: 10,
                  alignItems: "center",
                }}
              >
                <KeyBtn
                  label="STOP"
                  wide
                  danger
                  active={stopHeld} // ✅ ให้มีสถานะกดค้างเหมือนปุ่มอื่น
                  disabled={!connected}
                  onDown={stopHoldPress} // ✅ กดค้าง
                  onUp={stopHoldRelease} // ✅ ปล่อยแล้ว resume ถ้ามีปุ่มค้างอยู่
                />
              </div>

              <div
                style={{
                  marginTop: 10,
                  fontSize: 12,
                  color: "var(--muted)",
                  lineHeight: 1.35,
                }}
              ></div>
            </div>

            {/* Controls (ล่างสุด) ✅ ต้องมี div ครอบ ไม่งั้น </div> จะเพี้ยน */}
            <div style={{ marginTop: "auto", display: "grid", gap: 10 }}>
              <button
                onClick={async () => {
                  if (isPaused) {
                    setIsPaused(false);
                    showToast("RESUME (unlocked)", "success");
                  } else {
                    await sendCmd({ cmd: "STOP" });
                    setIsPaused(true);
                    showToast("STOP (paused)", "success");
                  }
                }}
                className="btn"
                style={{
                  width: "100%",
                  padding: 14,
                  borderRadius: 12,
                  background: isPaused
                    ? "rgba(22,163,74,0.20)"
                    : "rgba(255,255,255,0.04)",
                  borderColor: "var(--border)",
                }}
              >
                {isPaused ? (
                  <>
                    <Icons.Play /> RESUME
                  </>
                ) : (
                  <>
                    <Icons.Pause /> PAUSE ROBOT
                  </>
                )}
              </button>

              <button
                onClick={() => {
                  sendCmd({ cmd: "STOP" });
                  setIsPaused(false);
                  showToast("EMERGENCY STOP TRIGGERED", "error");
                }}
                className="btn btnDanger"
                style={{
                  width: "100%",
                  padding: 16,
                  borderRadius: 12,
                }}
              >
                ⛔ EMERGENCY STOP
              </button>
            </div>
          </div>

          {/* Right: Visualization */}
          <div
            className="panel"
            style={{
              padding: 0,
              position: "relative",
              overflow: "hidden",
              display: "flex",
              flexDirection: "column",
            }}
          >
            {/* Map Toolbar */}
            <div
              style={{
                position: "absolute",
                top: 12,
                right: 12,
                display: "flex",
                gap: 8,
                zIndex: 10,
              }}
            >
              <button
                onClick={() => setShowTags(!showTags)}
                className="btn btnGhost"
                style={{
                  height: 36,
                  padding: "0 12px",
                  borderRadius: 10,
                  fontSize: 12,
                }}
              >
                {showTags ? "Hide tags" : "Show tags"}
              </button>
            </div>

            {/* Canvas */}
            <div
              style={{
                flex: 1,
                background: "var(--surface2)",
                position: "relative",
              }}
            >
              <canvas
                ref={canvasRef}
                width={1200}
                height={700}
                style={{
                  width: "100%",
                  height: "100%",
                  objectFit: "contain",
                  display: "block",
                  touchAction: "none",
                }}
                onPointerMove={(e) => {
                  const rect = e.target.getBoundingClientRect();
                  const scaleX = e.target.width / rect.width;
                  const scaleY = e.target.height / rect.height;
                  mouseRef.current = {
                    x: (e.clientX - rect.left) * scaleX,
                    y: (e.clientY - rect.top) * scaleY,
                    active: true,
                  };
                }}
                onPointerLeave={() => {
                  mouseRef.current.active = false;
                }}
              />
            </div>

            {/* Zoom Footer */}
            <div
              style={{
                padding: "12px 14px",
                borderTop: "1px solid var(--border)",
                display: "flex",
                justifyContent: "space-between",
                alignItems: "center",
                background: "var(--surface)",
              }}
            >
              <span
                style={{ fontSize: 13, color: "var(--muted)", fontWeight: 700 }}
              >
                MAP VIEW: {(scale * 100).toFixed(0)}%
              </span>

              <div
                style={{
                  display: "flex",
                  alignItems: "center",
                  gap: 10,
                  width: 240,
                }}
              >
                <button
                  className="btn btnGhost"
                  style={{ height: 30, width: 34, borderRadius: 10 }}
                  onClick={() =>
                    setScale((s) => Math.max(ZOOM_MIN, s - ZOOM_STEP))
                  }
                >
                  −
                </button>

                <input
                  type="range"
                  min={ZOOM_MIN}
                  max={ZOOM_MAX}
                  step={ZOOM_STEP}
                  value={scale}
                  onChange={(e) => setScale(parseFloat(e.target.value))}
                />

                <button
                  className="btn btnGhost"
                  style={{ height: 30, width: 34, borderRadius: 10 }}
                  onClick={() =>
                    setScale((s) => Math.min(ZOOM_MAX, s + ZOOM_STEP))
                  }
                >
                  +
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </>
  );
}

/* ================== SUB COMPONENTS ================== */
const Divider = () => (
  <div style={{ width: 1, height: 34, background: "var(--border)" }} />
);

const Metric = ({ label, value, unit }) => (
  <div>
    <div
      style={{
        fontSize: 11,
        fontWeight: 700,
        color: "var(--muted)",
        letterSpacing: 0.5,
        marginBottom: 4,
      }}
    >
      {label}
    </div>
    <div
      style={{
        fontSize: 22,
        fontWeight: 500,
        fontFamily: "'JetBrains Mono', monospace",
        color: "var(--text)",
      }}
    >
      {value}
      {unit ? (
        <span style={{ fontSize: 13, color: "var(--muted)", marginLeft: 4 }}>
          {unit}
        </span>
      ) : null}
    </div>
  </div>
);
const KeyBtn = ({
  label,
  active,
  disabled,
  onDown,
  onUp,
  wide = false,
  danger = false,
}) => {
  const baseBg = danger ? "rgba(220,38,38,0.20)" : "rgba(255,255,255,0.04)";
  const activeBg = danger ? "rgba(220,38,38,0.35)" : "rgba(59,130,246,0.22)";

  const baseBd = danger ? "rgba(220,38,38,0.35)" : "var(--border)";
  const activeBd = danger ? "rgba(220,38,38,0.60)" : "rgba(59,130,246,0.55)";

  return (
    <button
      className="btn"
      disabled={disabled}
      onPointerDown={(e) => {
        e.preventDefault();
        e.currentTarget.setPointerCapture?.(e.pointerId);
        onDown?.();
      }}
      onPointerUp={(e) => {
        e.preventDefault();
        onUp?.();
      }}
      onPointerLeave={() => onUp?.()}
      onPointerCancel={() => onUp?.()}
      style={{
        height: 44,
        borderRadius: 12,
        fontWeight: 800,
        letterSpacing: 0.5,
        background: active ? activeBg : baseBg,
        borderColor: active ? activeBd : baseBd,
        width: wide ? "100%" : "auto",
        gridColumn: wide ? "1 / -1" : undefined,
      }}
      title={label}
    >
      {label}
    </button>
  );
};
