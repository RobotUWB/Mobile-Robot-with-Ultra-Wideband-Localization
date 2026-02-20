import React, { useEffect, useRef, useState } from "react";
import SettingsPanel from "./SettingsPanel";
import Joystick from "./Joystick";
import MapCanvas from "./MapCanvas";

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
// Robot Controller STM32 (192.168.88.115) -> Proxy: /robot
const CMD_BASE = "/robot";
const CMD_URL = `${CMD_BASE}/cmd`;

const FIELD_W = 3000;
const FIELD_H = 2000;

// WebSocket Control (192.168.88.115:81)
const CMD_WS_URL = "ws://192.168.88.115:81";
// WebSocket UWB (192.168.88.99:81)
const UWB_WS_URL = "ws://192.168.88.99:81";
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
  const [scale, setScale] = useState(0.15);

  const [anchors, setAnchors] = useState([
    { id: "A1", x_mm: 0, y_mm: 0 },
    { id: "A2", x_mm: 0, y_mm: 2000 },
    { id: "A3", x_mm: 3000, y_mm: 0 },
    { id: "A4", x_mm: 3000, y_mm: 2000 },
  ]);

  const [ranges, setRanges] = useState({ A1: 0, A2: 0, A3: 0, A4: 0 });

  // WebSocket for Control
  const isMounted = useRef(true);
  const wsCmdRef = useRef(null);
  const [wsConnected, setWsConnected] = useState(false);

  // WebSocket for UWB Data
  const uwbWsRef = useRef(null);

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
  const [navTarget, setNavTarget] = useState(null); // { x_m, y_m } — confirmed nav destination

  /* --- Refs for Animation Loop --- */
  const anchorsRef = useRef(anchors);
  const missedHeartbeatsRef = useRef(0); // ✅ Heartbeat Counter

  useEffect(() => {
    anchorsRef.current = anchors;
  }, [anchors]);

  useEffect(() => {
    localStorage.setItem("uwb_yaw_offset", yawOffset.toString());
  }, [yawOffset]);

  /* --- Helper: Toast --- */
  const showToast = (msg, type = "info") => {
    setToast({ show: true, msg, type });
    setTimeout(() => setToast((prev) => ({ ...prev, show: false })), 2600);
  };

  /* --- API Actions (Control Fallback) --- */
  const sendCmd = async ({ cmd }) => {
    try {
      const res = await fetch(CMD_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ cmd }),
        cache: "no-store",
      });
      return res.ok;
    } catch (e) {
      console.error("CMD fetch failed:", e);
      return false;
    }
  };

  /* --- WebSocket Actions --- */
  const calT1 = () => {
    const x = parseFloat(refX);
    const y = parseFloat(refY);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      showToast("REF must be numbers", "error");
      return;
    }

    if (uwbWsRef.current && uwbWsRef.current.readyState === WebSocket.OPEN) {
      // ใช้ CAL x y สำหรับ Single point (ตาม WebWorker(2).cpp)
      uwbWsRef.current.send(`CAL ${x} ${y}`);
      showToast("CAL T1 (Single) started", "success");
    } else {
      showToast("UWB WebSocket not connected", "error");
    }
  };

  const saveT1 = () => {
    if (uwbWsRef.current && uwbWsRef.current.readyState === WebSocket.OPEN) {
      uwbWsRef.current.send("SAVE");
      showToast("SAVE T1 Sent", "success");
    } else {
      showToast("UWB WebSocket not connected", "error");
    }
  };

  const resetT1 = () => {
    if (uwbWsRef.current && uwbWsRef.current.readyState === WebSocket.OPEN) {
      uwbWsRef.current.send("RESET");
      showToast("RESET T1 Sent", "success");
    } else {
      showToast("UWB WebSocket not connected", "error");
    }
  };

  const calDirection = () => {
    // Check if WS is connected for hardware ZERO
    if (wsCmdRef.current && wsCmdRef.current.readyState === WebSocket.OPEN) {
      wsCmdRef.current.send("SET_ZERO");
      setYawOffset(0); // Hardware is zeroed, so local offset is 0
      // Toast will come from ESP32 ACK response
    } else {
      // Fallback: Local offset only
      const currentYaw = pose.yaw || 0;
      setYawOffset(currentYaw);
      showToast("CAL DIRECTION (LOCAL ONLY)", "warning");
    }
  };

  /* ================== MANUAL DRIVE LOGIC ================== */
  // Unified active keys stack (Last in = Active)
  const pressedKeysRef = useRef([]);

  // ✅ UI State for button highlights (Restored to fix white screen)
  const [driveKey, setDriveKey] = useState(null); // 'W', 'A', 'S', 'D' or null
  const [rotKey, setRotKey] = useState(null);     // 'Q', 'E' or null

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
    }
  };

  // ✅ New: Send command every 200ms based on active key stack
  useEffect(() => {
    const intervalId = setInterval(() => {
      if (isPausedRef2.current) return;

      // Priority 1: STOP held (Spacebar) - always wins
      if (stopHeldRef.current) {
        sendDriveCmd(DRIVE_STOP_CMD);
        return;
      }

      // Priority 2: Last pressed key (WASD or QE)
      const stack = pressedKeysRef.current;
      if (stack.length > 0) {
        const activeKey = stack[stack.length - 1];
        // Check if it's a move key or rotate key
        if (DRIVE_CMDS[activeKey]) {
          sendDriveCmd(DRIVE_CMDS[activeKey]);
        } else if (ROT_CMDS[activeKey]) {
          sendDriveCmd(ROT_CMDS[activeKey]);
        }
      }
    }, 200);

    return () => clearInterval(intervalId);
  }, []);

  // Helper to sync UI state with stack
  const updateActiveKeys = () => {
    const stack = pressedKeysRef.current;
    if (stack.length === 0) {
      setDriveKey(null);
      setRotKey(null);
      return;
    }
    const last = stack[stack.length - 1];
    if (["W", "A", "S", "D"].includes(last)) {
      setDriveKey(last);
      setRotKey(null);
    } else if (["Q", "E"].includes(last)) {
      setDriveKey(null);
      setRotKey(last);
    }
  };

  const handleKeyPress = (k) => {
    if (!k) return;
    const stack = pressedKeysRef.current;
    if (!stack.includes(k)) {
      stack.push(k);
      updateActiveKeys();
    }
  };

  const handleKeyRelease = (k) => {
    if (!k) return;
    pressedKeysRef.current = pressedKeysRef.current.filter((x) => x !== k);
    updateActiveKeys();

    // If stack becomes empty, send STOP immediately
    if (pressedKeysRef.current.length === 0 && !stopHeldRef.current) {
      sendDriveCmd(DRIVE_STOP_CMD);
    }
  };

  // ✅ Wrappers for JSX buttons (Fix ReferenceError)
  const manualPress = (k) => handleKeyPress(k);
  const manualRelease = (k) => handleKeyRelease(k);
  const rotatePress = (k) => handleKeyPress(k);
  const rotateRelease = () => {
    // JSX calls this without args for Q/E release
    handleKeyRelease("Q");
    handleKeyRelease("E");
  };

  const stopHoldPress = () => {
    stopHeldRef.current = true;
    setStopHeld(true);
    sendDriveCmd(DRIVE_STOP_CMD);
  };

  const stopHoldRelease = () => {
    stopHeldRef.current = false;
    setStopHeld(false);
    // If keys are still held, the interval will pick them up in <200ms
    // Or we can force trigger now:
    const stack = pressedKeysRef.current;
    if (stack.length > 0) {
      const activeKey = stack[stack.length - 1];
      if (DRIVE_CMDS[activeKey]) sendDriveCmd(DRIVE_CMDS[activeKey]);
      else if (ROT_CMDS[activeKey]) sendDriveCmd(ROT_CMDS[activeKey]);
    }
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
        if (e.repeat) return;
        e.preventDefault();
        handleKeyPress(r);
        return;
      }

      const k = codeToMoveKey(e.code);
      if (k) {
        if (e.repeat) return;
        e.preventDefault();
        handleKeyPress(k);
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
        handleKeyRelease(r);
        return;
      }

      const k = codeToMoveKey(e.code);
      if (k) {
        e.preventDefault();
        handleKeyRelease(k);
      }
    };

    const onBlur = () => {
      pressedKeysRef.current = [];
      if (!stopHeldRef.current) sendDriveCmd(DRIVE_STOP_CMD);
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

  /* --- WebSocket for UWB Data (Position + Anchors) --- */
  useEffect(() => {
    isMounted.current = true;
    let uwbReconnectTimeout = null;

    const connectWsUwb = () => {
      if (!isMounted.current) return;
      console.log("WS-UWB: Connecting to", UWB_WS_URL);
      const ws = new WebSocket(UWB_WS_URL);
      uwbWsRef.current = ws;

      ws.onopen = () => {
        if (!isMounted.current) {
          ws.close();
          return;
        }
        console.log("WS-UWB: Connected");
        setConnected(true);
        missedHeartbeatsRef.current = 0;
      };

      ws.onmessage = (event) => {
        try {
          const j = JSON.parse(event.data);
          missedHeartbeatsRef.current = 0;

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
            setRanges(newRanges);
          }

          // Calibration state (cs)
          const cs = Number(j.cs);
          if (Number.isFinite(cs)) setCalState(cs);

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
        } catch (e) {
          console.error("WS-UWB parse error:", e);
        }
      };

      ws.onclose = () => {
        if (!isMounted.current) return;
        console.log("WS-UWB: Closed, reconnecting...");
        setConnected(false);
        uwbReconnectTimeout = setTimeout(connectWsUwb, 2000);
      };

      ws.onerror = (e) => {
        console.error("WS-UWB Error:", e);
        ws.close();
      };
    };

    connectWsUwb();

    return () => {
      isMounted.current = false;
      if (uwbReconnectTimeout) clearTimeout(uwbReconnectTimeout);
      if (uwbWsRef.current) {
        uwbWsRef.current.onclose = null; // Prevent re-triggering cleanup logic
        uwbWsRef.current.close();
      }
    };
  }, []);

  /* --- WebSocket for Robot Control (Heading + Commands) --- */
  useEffect(() => {
    let reconnectTimeout = null;
    let heartbeatInterval = null;

    const connectWsCmd = () => {
      if (!isMounted.current) return;
      if (wsCmdRef.current && wsCmdRef.current.readyState === WebSocket.OPEN)
        return;

      console.log("WS-CMD: Connecting to", CMD_WS_URL);
      const ws = new WebSocket(CMD_WS_URL);
      wsCmdRef.current = ws;

      ws.onopen = () => {
        if (!isMounted.current) {
          ws.close();
          return;
        }
        console.log("WS-CMD: Connected");
        setWsConnected(true);

        // Consolidated heartbeat: Send "PING" to reset timeout & "H" for heartbeat
        // WEB_TIMEOUT_MS on ESP32 is 300ms, so we send every 100ms
        heartbeatInterval = setInterval(() => {
          if (ws.readyState === WebSocket.OPEN) {
            ws.send("PING");
            ws.send("H");
          }
        }, 100);
      };

      ws.onmessage = (event) => {
        try {
          if (event.data === "PONG") return;
          const j = JSON.parse(event.data);

          // ✅ Handle ACK responses from ESP32
          if (j.type === "ack") {
            const status = j.status === "success" ? "success" : "error";
            showToast(`${j.cmd}: ${j.status}`, status);
            return;
          }

          const angle = Number(j.angle);
          if (Number.isFinite(angle)) {
            setPose((prev) => ({ ...prev, yaw: angle }));
          }
        } catch (e) {
          // Non-JSON ack messages ignore
        }
      };

      ws.onclose = () => {
        setWsConnected(false);
        wsCmdRef.current = null;
        if (heartbeatInterval) clearInterval(heartbeatInterval);
        if (!isMounted.current) return;
        console.log("WS-CMD: Closed, reconnecting...");
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
      if (heartbeatInterval) clearInterval(heartbeatInterval);
      if (wsCmdRef.current) {
        wsCmdRef.current.onclose = null;
        wsCmdRef.current.close();
      }
    };
  }, []);

  /* ================== RENDER ================== */
  const toastBg =
    toast.type === "error"
      ? "rgba(220,38,38,0.95)"
      : toast.type === "success"
        ? "rgba(22,163,74,0.95)"
        : "rgba(59,130,246,0.95)";

  const handleMapClick = (x, y) => {
    // x, y are in meters
    const cmdVal = `GOTO:${x.toFixed(2)}:${y.toFixed(2)}`;
    // Send via WebSocket (Control ESP)
    if (wsCmdRef.current && wsCmdRef.current.readyState === WebSocket.OPEN) {
      wsCmdRef.current.send(cmdVal);
      setNavTarget({ x_m: x, y_m: y }); // ✅ Show marker on map
      showToast(`Going to X:${x.toFixed(2)} Y:${y.toFixed(2)}`, "info");
    } else {
      showToast("Control WS not connected", "error");
    }
  };

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
            <SettingsPanel
              ranges={ranges}
              calState={calState}
              CAL_TEXT={CAL_TEXT}
              CAL_COLOR={CAL_COLOR}
              refX={refX}
              setRefX={setRefX}
              refY={refY}
              setRefY={setRefY}
              calT1={calT1}
              calDirection={calDirection}
              saveT1={saveT1}
              resetT1={resetT1}
              connected={connected}
            />

            {/* Manual Drive (TEST) */}
            <Joystick
              rotKey={rotKey}
              driveKey={driveKey}
              stopHeld={stopHeld}
              connected={connected}
              rotatePress={rotatePress}
              rotateRelease={rotateRelease}
              manualPress={manualPress}
              manualRelease={manualRelease}
              stopHoldPress={stopHoldPress}
              stopHoldRelease={stopHoldRelease}
            />

            {/* Navigation Target */}
            {navTarget && (
              <div className="panel" style={{ padding: "14px 16px" }}>
                <div className="sectionTitle">NAVIGATION TARGET</div>
                <div style={{
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "space-between",
                  gap: 10,
                }}>
                  <div style={{
                    fontFamily: "'JetBrains Mono', monospace",
                    fontSize: 14,
                    color: "var(--text)",
                  }}>
                    🔴 X: {navTarget.x_m.toFixed(2)}  Y: {navTarget.y_m.toFixed(2)}
                  </div>
                  <button
                    className="btn btnGhost"
                    style={{ height: 28, padding: "0 10px", borderRadius: 8, fontSize: 11 }}
                    onClick={() => setNavTarget(null)}
                  >
                    CLEAR
                  </button>
                </div>
              </div>
            )}
            {/* Controls (ล่างสุด) */}
            <div style={{ marginTop: "auto", display: "grid", gap: 10 }}>
              <button
                onClick={async () => {
                  if (isPaused) {
                    setIsPaused(false);
                    showToast("RESUME (unlocked)", "success");
                  } else {
                    sendDriveCmd("STOP");
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
                  // Prefer WebSocket for Stop if available, else sendCmd logic (which falls back)
                  // But sendCmd calls fetch CMD_URL which is HTTP.
                  // User asked: "wsCmd.send('STOP')"
                  // sendDriveCmd does check WS first.
                  sendDriveCmd("STOP");
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
          <MapCanvas
            anchors={anchors}
            ranges={ranges}
            pose={pose}
            yawOffset={yawOffset}
            showTags={showTags}
            setShowTags={setShowTags}
            scale={scale}
            setScale={setScale}
            ZOOM_MIN={ZOOM_MIN}
            ZOOM_MAX={ZOOM_MAX}
            ZOOM_STEP={ZOOM_STEP}
            FIELD_W={FIELD_W}
            FIELD_H={FIELD_H}
            onMapClick={handleMapClick}
            navTarget={navTarget}
          />
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
