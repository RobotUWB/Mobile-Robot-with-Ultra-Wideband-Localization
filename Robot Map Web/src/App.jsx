import React, { useEffect, useRef, useState } from "react";

/* ================== GLOBAL STYLES & ICONS ================== */
const Icons = {
  Play: () => (
    <svg width="16" height="16" fill="currentColor" viewBox="0 0 24 24">
      <path d="M5 3l14 9-14 9V3z" />
    </svg>
  ),
  Pause: () => (
    <svg width="16" height="16" fill="currentColor" viewBox="0 0 24 24">
      <path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z" />
    </svg>
  ),
  Refresh: () => (
    <svg width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2" viewBox="0 0 24 24">
      <path d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
    </svg>
  ),
  Wifi: () => (
    <svg width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2" viewBox="0 0 24 24">
      <path d="M5 12.55a11 11 0 0114.08 0M1.64 9a15 15 0 0120.72 0M8.27 16a6 6 0 017.46 0M12 20h.01" />
    </svg>
  ),
};

const GlobalStyles = () => (
  <style>{`
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;700&display=swap');
    
    :root {
      --bg-dark: #0f172a;
      --bg-panel: rgba(30, 41, 59, 0.7);
      --bg-card: rgba(15, 23, 42, 0.6);
      --accent: #38bdf8;
      --success: #22c55e;
      --danger: #ef4444;
      --warning: #eab308;
      --text-main: #f1f5f9;
      --text-muted: #94a3b8;
      --border: rgba(255,255,255,0.08);
    }

    body {
      margin: 0;
      background-color: #020617;
      background-image: radial-gradient(circle at top center, #1e293b 0%, #020617 100%);
      font-family: 'Inter', sans-serif;
      color: var(--text-main);
      -webkit-font-smoothing: antialiased;
      overflow-x: hidden;
    }

    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      background: transparent;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      height: 14px;
      width: 14px;
      border-radius: 50%;
      background: var(--accent);
      cursor: pointer;
      margin-top: -5px;
      box-shadow: 0 0 8px rgba(56, 189, 248, 0.6);
      transition: transform 0.1s;
    }
    input[type=range]::-webkit-slider-thumb:hover { transform: scale(1.2); }
    input[type=range]::-webkit-slider-runnable-track {
      width: 100%;
      height: 4px;
      cursor: pointer;
      background: #334155;
      border-radius: 2px;
    }

    .btn-base {
      transition: all 0.2s ease;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      font-weight: 600;
      outline: none;
      user-select: none;
    }
    .btn-base:hover { filter: brightness(1.1); transform: translateY(-1px); }
    .btn-base:active { transform: translateY(1px); }
    
    .glass-panel {
      background: var(--bg-panel);
      backdrop-filter: blur(16px);
      border: 1px solid var(--border);
      box-shadow: 0 4px 24px -1px rgba(0,0,0,0.2);
    }

    .mono-font { font-family: 'JetBrains Mono', monospace; }

    input.cal-in {
      border: 1px solid var(--border);
      background: rgba(2, 6, 23, 0.55);
      color: #e2e8f0;
      outline: none;
      border-radius: 10px;
      padding: 8px 10px;
      text-align: center;
      font-family: 'JetBrains Mono', monospace;
      font-size: 12px;
      width: 76px;
    }
    
    @keyframes pulse-ring {
      0% { transform: scale(0.8); opacity: 0.5; }
      100% { transform: scale(2); opacity: 0; }
    }
    .pulse-dot { position: relative; }
    .pulse-dot::before {
      content: '';
      position: absolute;
      left: 0; top: 0; width: 100%; height: 100%;
      background: inherit; border-radius: 50%;
      z-index: -1;
      animation: pulse-ring 2s infinite;
    }
    @keyframes spin { 100% { transform: rotate(360deg); } }
    .spin { animation: spin 1s linear infinite; }

    ::-webkit-scrollbar { width: 8px; }
    ::-webkit-scrollbar-track { background: transparent; }
    ::-webkit-scrollbar-thumb { background: #334155; border-radius: 4px; }
    ::-webkit-scrollbar-thumb:hover { background: #475569; }
  `}</style>
);

/* ================== CONFIG ================== */
const ESP32_IP = "192.168.88.69"; // (ตอนนี้ใช้ proxy แล้ว เลยไม่ต้องใช้ตรง ๆ)
const API_URL = "/api/json";
const CMD_URL = "/api/cmd";

const FIELD_W = 3000;
const FIELD_H = 2000;

const ZOOM_MIN = 0.05,
  ZOOM_MAX = 0.40,
  ZOOM_STEP = 0.01;

// ✅ แบบเพื่อน: Tag2 = Tag1 + 0.40m (fallback ถ้า ESP ยังไม่ส่ง t2)
const TAG2_OFFSET_M = 0.40;

/* ================== APP COMPONENT ================== */
export default function App() {
  // ===== Calibration UI =====
  const [refX, setRefX] = useState("1.00");
  const [refY, setRefY] = useState("1.50");
  const [calState, setCalState] = useState(0); // 0..4 from ESP32 json.cs

  const CAL_TEXT = ["READY", "CALIBRATING...", "SUCCESS!", "FAILED", "RESET DONE"];
  const CAL_COLOR = ["#aaa", "#eab308", "#10b981", "#ef4444", "#3b82f6"];

  /* --- State: Canvas & Data --- */
  const canvasRef = useRef(null);
  const [scale, setScale] = useState(0.15);

  const [anchors, setAnchors] = useState([
    { id: "A1", x_mm: 0, y_mm: 2000 },
    { id: "A2", x_mm: 3000, y_mm: 2000 },
    { id: "A3", x_mm: 0, y_mm: 0 },
    { id: "A4", x_mm: 3000, y_mm: 0 },
  ]);

  const [ranges, setRanges] = useState({ A1: 0, A2: 0, A3: 0, A4: 0 });

  // Tag1
  const [pose, setPose] = useState({ x_mm: 0, y_mm: 0 });

  // Tag2
  const [pose2, setPose2] = useState({ x_mm: 0, y_mm: 0, ok: false });
  const pose2Ref = useRef(pose2);
  useEffect(() => {
    pose2Ref.current = pose2;
  }, [pose2]);

  // RMSE (ถ้ามีใน json)
  const [rmse, setRmse] = useState(null);

  /* --- State: System --- */
  const [connected, setConnected] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [showTags, setShowTags] = useState(true);
  const [refreshKey, setRefreshKey] = useState(0);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [toast, setToast] = useState({ show: false, msg: "", type: "info" });

  /* --- Refs for Animation Loop --- */
  const scaleRef = useRef(scale);
  const poseRef = useRef(pose);
  const anchorsRef = useRef(anchors);
  const showTagsRef = useRef(showTags);
  const rangesRef = useRef(ranges);

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

  /* --- Helper: Toast --- */
  const showToast = (msg, type = "info") => {
    setToast({ show: true, msg, type });
    setTimeout(() => setToast((prev) => ({ ...prev, show: false })), 3000);
  };

  /* --- API Actions --- */
  const sendCmd = async (payload) => {
    try {
      await fetch(CMD_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      return true;
    } catch (e) {
      console.error(e);
      showToast("Command Failed: Check Connection", "error");
      return false;
    }
  };

  const handleRefresh = () => {
    setIsRefreshing(true);
    setConnected(false);
    setTimeout(() => {
      setRefreshKey((k) => k + 1);
      setIsRefreshing(false);
      showToast("Reconnecting...", "info");
    }, 800);
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
      const url = `/api/cal?x=${encodeURIComponent(x)}&y=${encodeURIComponent(y)}`;
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
      const r = await fetch("/api/save");
      if (!r.ok) throw new Error("SAVE failed");
      showToast("SAVE T1 OK", "success");
    } catch (e) {
      console.error(e);
      showToast("SAVE T1 failed (check /save endpoint)", "error");
    }
  };

  const resetT1 = async () => {
    try {
      const r = await fetch("/api/reset");
      if (!r.ok) throw new Error("RESET failed");
      showToast("RESET T1 OK", "success");
    } catch (e) {
      console.error(e);
      showToast("RESET T1 failed (check /reset endpoint)", "error");
    }
  };

  /* --- Polling Data Loop --- */
  useEffect(() => {
    const fetchData = async () => {
      try {
        const res = await fetch(API_URL);
        if (!res.ok) throw new Error("Network error");
        const j = await res.json();

        setConnected(true);

        // ==========================================
        // 🛠️ Calibration (ใช้กับ Tag1/Tag2 เหมือนกัน)
        // ==========================================
        const SWAP_XY = false;
        const INVERT_X = false;
        const INVERT_Y = false;

        const applyCal = (x_m, y_m) => {
          let rawX = x_m * 1000; // m -> mm
          let rawY = y_m * 1000;

          let finalX = SWAP_XY ? rawY : rawX;
          let finalY = SWAP_XY ? rawX : rawY;

          if (INVERT_X) finalX = FIELD_W - finalX;
          if (INVERT_Y) finalY = FIELD_H - finalY;

          return { x_mm: finalX, y_mm: finalY };
        };

        // 1) Tag1 Update (จาก j.x, j.y)
        const xVal = Number(j.x);
        const yVal = Number(j.y);

        const tag1Ok = Number.isFinite(xVal) && Number.isFinite(yVal) && xVal !== -1 && yVal !== -1;

        if (tag1Ok) {
          setPose(applyCal(xVal, yVal));
        }

        // 2) Tag2 Update
        // - ถ้า ESP ส่งมาเป็น j.t2 (ok==1) -> ใช้จริง
        // - ถ้าไม่ส่ง -> ใช้ offset แบบเพื่อน (x + 0.40, y)
        let t2_ok = false;
        let t2x_m = 0;
        let t2y_m = 0;

        if (j.t2 && Number(j.t2.ok) === 1) {
          const x2 = Number(j.t2.x);
          const y2 = Number(j.t2.y);
          if (Number.isFinite(x2) && Number.isFinite(y2)) {
            t2_ok = true;
            t2x_m = x2;
            t2y_m = y2;
          }
        } else if (tag1Ok) {
          // fallback แบบโค้ดเพื่อน
          t2_ok = true;
          t2x_m = xVal + TAG2_OFFSET_M;
          t2y_m = yVal;
        }

        if (t2_ok) {
          const p2 = applyCal(t2x_m, t2y_m);
          setPose2({ x_mm: p2.x_mm, y_mm: p2.y_mm, ok: true });
        } else {
          setPose2((prev) => (prev.ok ? { ...prev, ok: false } : prev));
        }

        // 3) Ranges Update
        if (j.a && Array.isArray(j.a)) {
          setRanges({
            A1: parseFloat(j.a[0]) || 0,
            A2: parseFloat(j.a[1]) || 0,
            A3: parseFloat(j.a[2]) || 0,
            A4: parseFloat(j.a[3]) || 0,
          });
        }

        // 4) Anchor XY Update
        if (j.anch_xy && Array.isArray(j.anch_xy)) {
          const newAnchors = j.anch_xy.map((p, i) => ({
            id: `A${i + 1}`,
            x_mm: p[0] * 1000,
            y_mm: p[1] * 1000,
          }));
          if (newAnchors.length >= 3) setAnchors(newAnchors);
        }

        // 5) RMSE (optional)
        const r = Number(j.rmse);
        if (Number.isFinite(r)) setRmse(r);

        // ✅ 6) Calibration state (optional) 0..4
        const cs = Number(j.cs);
        if (Number.isFinite(cs)) setCalState(Math.max(0, Math.min(4, cs)));
      } catch (err) {
        setConnected(false);
      }
    };

    const intervalId = setInterval(fetchData, 100);
    return () => clearInterval(intervalId);
  }, [refreshKey]);

  /* --- Canvas Drawing --- */
  useEffect(() => {
    const cvs = canvasRef.current;
    if (!cvs) return;
    const ctx = cvs.getContext("2d");
    let raf;

    const mmToPx = (mm, s) => mm * s;

    const getBoundsFromAnchors = (anchorsList) => {
      if (anchorsList && anchorsList.length) {
        let minX = Infinity,
          maxX = -Infinity,
          minY = Infinity,
          maxY = -Infinity;

        for (const a of anchorsList) {
          if (typeof a.x_mm !== "number" || typeof a.y_mm !== "number") continue;
          minX = Math.min(minX, a.x_mm);
          maxX = Math.max(maxX, a.x_mm);
          minY = Math.min(minY, a.y_mm);
          maxY = Math.max(maxY, a.y_mm);
        }

        if (isFinite(minX) && isFinite(maxX) && isFinite(minY) && isFinite(maxY) && minX !== maxX && minY !== maxY) {
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

    const drawTriangle = (x, y, size, color, glow, pointUp) => {
      ctx.shadowBlur = 18;
      ctx.shadowColor = glow;

      ctx.fillStyle = color;
      ctx.beginPath();
      if (pointUp) {
        ctx.moveTo(x, y - size);
        ctx.lineTo(x + size * 0.85, y + size * 0.7);
        ctx.lineTo(x - size * 0.85, y + size * 0.7);
      } else {
        ctx.moveTo(x, y + size);
        ctx.lineTo(x + size * 0.85, y - size * 0.7);
        ctx.lineTo(x - size * 0.85, y - size * 0.7);
      }
      ctx.closePath();
      ctx.fill();

      ctx.shadowBlur = 0;
      ctx.strokeStyle = "rgba(255,255,255,0.20)";
      ctx.lineWidth = 1.5;
      ctx.stroke();
    };

    const drawTagLabel = (text, x, y, above = true) => {
      ctx.font = "700 12px Inter";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";

      const padX = 10;
      const h = 22;
      const w = ctx.measureText(text).width + padX * 2;

      const bx = x - w / 2;
      const by = above ? y - 26 - h : y + 26;

      ctx.fillStyle = "rgba(15, 23, 42, 0.75)";
      ctx.strokeStyle = "rgba(255,255,255,0.12)";
      ctx.lineWidth = 1;

      ctx.beginPath();
      ctx.roundRect(bx, by, w, h, 8);
      ctx.fill();
      ctx.stroke();

      ctx.fillStyle = "#e2e8f0";
      ctx.fillText(text, x, by + h / 2);
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
      ctx.fillStyle = "#0f172a";
      ctx.fillRect(0, 0, W, H);

      // Grid
      ctx.strokeStyle = "rgba(56, 189, 248, 0.05)";
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

      ctx.strokeStyle = "rgba(56, 189, 248, 0.3)";
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

        const colorMain = isOnline ? "#22c55e" : "#475569";
        const colorGlow = isOnline ? "rgba(34, 197, 94, 0.8)" : "transparent";
        const colorText = isOnline ? "#ffffff" : "#64748b";

        ctx.shadowBlur = isOnline ? 15 : 0;
        ctx.shadowColor = colorGlow;
        ctx.fillStyle = colorMain;
        ctx.beginPath();
        ctx.arc(px, py, isOnline ? 6 : 4, 0, Math.PI * 2);
        ctx.fill();
        ctx.shadowBlur = 0;

        if (show) {
          ctx.fillStyle = isOnline ? "rgba(34, 197, 94, 0.1)" : "transparent";
          ctx.strokeStyle = colorMain;
          ctx.lineWidth = 1;

          ctx.beginPath();
          if (!isOnline) ctx.setLineDash([4, 4]);
          ctx.arc(px, py, 14, 0, Math.PI * 2);
          ctx.stroke();
          ctx.fill();
          ctx.setLineDash([]);

          ctx.fillStyle = colorText;
          ctx.font = "bold 14px Inter";
          ctx.textAlign = "center";
          ctx.textBaseline = "alphabetic";

          const labelText = `${a.id} ${rangeVal.toFixed(2)}m`;
          ctx.fillText(labelText, px, py - 20);
        }
      }

      // =========================
      // Tag1 + Tag2 (เหมือนเพื่อน)
      // =========================
      const t1 = poseRef.current;
      const t2 = pose2Ref.current;

      const t1Finite = Number.isFinite(t1.x_mm) && Number.isFinite(t1.y_mm);
      if (t1Finite) {
        const p1 = toPx(t1.x_mm, t1.y_mm, cx, cy, s, bounds);

        if (t2 && t2.ok && Number.isFinite(t2.x_mm) && Number.isFinite(t2.y_mm)) {
          const p2 = toPx(t2.x_mm, t2.y_mm, cx, cy, s, bounds);

          // dashed line between tags
          ctx.beginPath();
          ctx.moveTo(p1.px, p1.py);
          ctx.lineTo(p2.px, p2.py);
          ctx.lineWidth = 2;
          ctx.strokeStyle = "rgba(0, 210, 255, 0.40)";
          ctx.setLineDash([6, 6]);
          ctx.stroke();
          ctx.setLineDash([]);

          // Tag1 (Front) - blue, point up
          drawTriangle(p1.px, p1.py, 10, "#3b82f6", "rgba(59,130,246,0.70)", true);
          // Tag2 (Back) - cyan, point down
          drawTriangle(p2.px, p2.py, 10, "#00d2ff", "rgba(0,210,255,0.70)", false);

          if (show) {
            drawTagLabel("Tag1 (Front)", p1.px, p1.py, true);
            drawTagLabel("Tag2 (Back)", p2.px, p2.py, false);
          }
        } else {
          // มีแค่ Tag1
          drawTriangle(p1.px, p1.py, 10, "#3b82f6", "rgba(59,130,246,0.70)", true);
          if (show) drawTagLabel("Tag1 (Front)", p1.px, p1.py, true);
        }

        // กล่อง X,Y แบบเดิม (โชว์ค่า Tag1)
        if (show) {
          ctx.font = "11px JetBrains Mono";
          ctx.textAlign = "center";
          ctx.textBaseline = "alphabetic";

          const label = `X:${(t1.x_mm / 1000).toFixed(2)} Y:${(t1.y_mm / 1000).toFixed(2)}`;
          const tw = ctx.measureText(label).width + 16;

          ctx.fillStyle = "rgba(15, 23, 42, 0.8)";
          ctx.strokeStyle = "rgba(255,255,255,0.2)";
          ctx.beginPath();
          ctx.roundRect(p1.px - tw / 2, p1.py + 24, tw, 24, 6);
          ctx.fill();
          ctx.stroke();

          ctx.fillStyle = "#fff";
          ctx.fillText(label, p1.px, p1.py + 42);
        }
      }

      raf = requestAnimationFrame(draw);
    };

    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, []);

  /* ================== RENDER ================== */
  return (
    <>
      <GlobalStyles />

      {/* --- Toast Notification --- */}
      <div
        style={{
          position: "fixed",
          top: 24,
          right: 24,
          zIndex: 99,
          background:
            toast.type === "error"
              ? "rgba(239, 68, 68, 0.9)"
              : toast.type === "success"
              ? "rgba(34, 197, 94, 0.9)"
              : "rgba(59, 130, 246, 0.9)",
          color: "#fff",
          padding: "12px 24px",
          borderRadius: 8,
          transform: toast.show ? "translateY(0)" : "translateY(-100px)",
          opacity: toast.show ? 1 : 0,
          transition: "all 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275)",
          fontWeight: 600,
          fontSize: 14,
          boxShadow: "0 10px 15px -3px rgba(0,0,0,0.3)",
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
          gap: 20,
        }}
      >
        {/* --- Header / Status Deck --- */}
        <header
          className="glass-panel"
          style={{
            borderRadius: 16,
            padding: "20px 24px",
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
          }}
        >
          <div style={{ display: "flex", gap: 16, alignItems: "center" }}>
            <div
              style={{
                width: 48,
                height: 48,
                background: "rgba(56, 189, 248, 0.1)",
                borderRadius: 12,
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
                color: "var(--accent)",
              }}
            >
              <Icons.Wifi />
            </div>
            <div>
              <h1 style={{ margin: 0, fontSize: 20, fontWeight: 700 }}>UWB Localization</h1>
              <div style={{ display: "flex", alignItems: "center", gap: 8, marginTop: 4 }}>
                <div
                  className="pulse-dot"
                  style={{
                    width: 8,
                    height: 8,
                    borderRadius: "50%",
                    background: connected ? "var(--success)" : "var(--danger)",
                  }}
                />
                <span style={{ fontSize: 15, color: connected ? "var(--success)" : "var(--danger)", fontWeight: 600 }}>
                  {connected ? "SYSTEM ONLINE" : "DISCONNECTED"}
                </span>
              </div>
            </div>
          </div>

          {/* Big Metrics */}
          <div style={{ display: "flex", gap: 32, alignItems: "center" }}>
            <Metric label="COORD X" value={(pose.x_mm / 1000).toFixed(2)} unit="m" />
            <div style={{ width: 1, background: "var(--border)" }} />
            <Metric label="COORD Y" value={(pose.y_mm / 1000).toFixed(2)} unit="m" />
            <div style={{ width: 1, background: "var(--border)" }} />
            <Metric label="RMSE" value={rmse == null ? "--" : rmse.toFixed(3)} unit="" />
          </div>
        </header>

        {/* --- Main Grid --- */}
        <div style={{ display: "grid", gridTemplateColumns: "300px 1fr", gap: 20, flex: 1 }}>
          {/* --- Left Sidebar (Controls) --- */}
          <div style={{ display: "flex", flexDirection: "column", gap: 20 }}>
            {/* 1. Anchors Status + CAL (รวมในกล่องเดียวกัน) */}
            <div className="glass-panel" style={{ borderRadius: 16, padding: 20 }}>
              <div style={{ fontSize: 15, fontWeight: 700, color: "var(--text-muted)", marginBottom: 16, letterSpacing: 1 }}>
                ANCHOR DISTANCES
              </div>

              <div style={{ display: "grid", gap: 12 }}>
                {Object.entries(ranges).map(([k, v]) => (
                  <div key={k} style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                    <span style={{ color: "#cbd5e1", fontSize: "16px", fontWeight: "700" }}>{k}</span>
                    <span className="mono-font" style={{ fontSize: "14px", color: "var(--accent)" }}>
                      {v.toFixed(2)}m
                    </span>
                  </div>
                ))}
              </div>

              {/* ===== TAG1 CALIBRATION (อยู่ใต้ ANCHOR DISTANCES) ===== */}
              <div style={{ marginTop: 16, paddingTop: 14, borderTop: "1px solid var(--border)" }}>
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 12 }}>
                  <div style={{ fontSize: 12, fontWeight: 800, color: "var(--text-muted)", letterSpacing: 1 }}>
                    TAG1 CALIBRATION
                  </div>

                  <div
                    style={{
                      fontSize: 12,
                      fontWeight: 900,
                      color: CAL_COLOR[calState] || "#aaa",
                      letterSpacing: 0.5,
                    }}
                  >
                    {CAL_TEXT[calState] || "READY"}
                  </div>
                </div>

                <div style={{ display: "flex", alignItems: "center", gap: 10, flexWrap: "wrap" }}>
                  <span style={{ fontSize: 12, color: "var(--text-muted)", fontWeight: 800 }}>REF:</span>

                  <input
                    className="cal-in"
                    value={refX}
                    onChange={(e) => setRefX(e.target.value)}
                    inputMode="decimal"
                    placeholder="1.00"
                    title="REF X (meter)"
                  />
                  <input
                    className="cal-in"
                    value={refY}
                    onChange={(e) => setRefY(e.target.value)}
                    inputMode="decimal"
                    placeholder="1.50"
                    title="REF Y (meter)"
                  />

                  <button
                    onClick={calT1}
                    disabled={!connected}
                    className="btn-base"
                    style={{
                      height: 34,
                      padding: "0 12px",
                      borderRadius: 10,
                      border: "none",
                      background: "rgba(59, 130, 246, 0.95)",
                      color: "#fff",
                      fontSize: 12,
                      opacity: connected ? 1 : 0.5,
                    }}
                  >
                    CAL T1
                  </button>

                  <button
                    onClick={saveT1}
                    disabled={!connected}
                    className="btn-base"
                    style={{
                      height: 34,
                      padding: "0 12px",
                      borderRadius: 10,
                      border: "none",
                      background: "rgba(34, 197, 94, 0.95)",
                      color: "#fff",
                      fontSize: 12,
                      opacity: connected ? 1 : 0.5,
                    }}
                  >
                    SAVE
                  </button>

                  <button
                    onClick={resetT1}
                    disabled={!connected}
                    className="btn-base"
                    style={{
                      height: 34,
                      padding: "0 12px",
                      borderRadius: 10,
                      border: "none",
                      background: "rgba(100, 116, 139, 0.95)",
                      color: "#fff",
                      fontSize: 12,
                      opacity: connected ? 1 : 0.5,
                    }}
                  >
                    RESET
                  </button>
                </div>

                <div style={{ marginTop: 10, fontSize: 11, color: "var(--text-muted)", lineHeight: 1.35 }}>
                  ใส่ค่า REF (เมตร) ของจุดอ้างอิงในแผนที่ แล้วกด <b>CAL T1</b>
                </div>
              </div>
            </div>

            {/* 2. Main Controls */}
            <div style={{ marginTop: "auto" }}>
              <button
                onClick={() => {
                  if (isPaused) {
                    sendCmd({ cmd: "RESUME" });
                    setIsPaused(false);
                  } else {
                    sendCmd({ cmd: "PAUSE" });
                    setIsPaused(true);
                  }
                }}
                className="btn-base"
                style={{
                  width: "100%",
                  padding: 16,
                  borderRadius: 12,
                  marginBottom: 12,
                  background: isPaused ? "var(--success)" : "#334155",
                  color: "#fff",
                  border: "none",
                  fontSize: 14,
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
                className="btn-base"
                style={{
                  width: "100%",
                  padding: 18,
                  borderRadius: 12,
                  background: "linear-gradient(135deg, #ef4444 0%, #dc2626 100%)",
                  color: "#fff",
                  border: "none",
                  fontSize: 16,
                  boxShadow: "0 4px 14px rgba(220, 38, 38, 0.4)",
                }}
              >
                ⛔ EMERGENCY STOP
              </button>
            </div>
          </div>

          {/* --- Right: Visualization --- */}
          <div
            className="glass-panel"
            style={{ borderRadius: 16, padding: 0, position: "relative", overflow: "hidden", display: "flex", flexDirection: "column" }}
          >
            {/* Map Toolbar */}
            <div style={{ position: "absolute", top: 16, right: 16, display: "flex", gap: 8, zIndex: 10 }}>
              <button
                onClick={handleRefresh}
                className="btn-base glass-panel"
                style={{
                  width: 36,
                  height: 36,
                  borderRadius: 8,
                  padding: 0,
                  color: isRefreshing ? "var(--accent)" : "var(--text-muted)",
                }}
              >
                <div className={isRefreshing ? "spin" : ""}>
                  <Icons.Refresh />
                </div>
              </button>
              <button
                onClick={() => setShowTags(!showTags)}
                className="btn-base glass-panel"
                style={{
                  padding: "0 12px",
                  height: 36,
                  borderRadius: 8,
                  fontSize: 12,
                  color: showTags ? "var(--accent)" : "var(--text-muted)",
                }}
              >
                {showTags ? "HIDE TAGS" : "SHOW TAGS"}
              </button>
            </div>

            {/* Canvas */}
            <div style={{ flex: 1, background: "#020617", position: "relative" }}>
              <canvas ref={canvasRef} width={1200} height={700} style={{ width: "100%", height: "100%", objectFit: "contain", display: "block" }} />
            </div>

            {/* Zoom Footer */}
            <div
              style={{
                padding: "12px 24px",
                borderTop: "1px solid var(--border)",
                display: "flex",
                justifyContent: "space-between",
                alignItems: "center",
                background: "rgba(15, 23, 42, 0.4)",
              }}
            >
              <span style={{ fontSize: 15, color: "var(--text-muted)" }}>MAP VIEW: {(scale * 100).toFixed(0)}%</span>
              <div style={{ display: "flex", alignItems: "center", gap: 12, width: 200 }}>
                <span style={{ fontSize: 16, cursor: "pointer" }} onClick={() => setScale((s) => Math.max(ZOOM_MIN, s - ZOOM_STEP))}>
                  -
                </span>
                <input type="range" min={ZOOM_MIN} max={ZOOM_MAX} step={ZOOM_STEP} value={scale} onChange={(e) => setScale(parseFloat(e.target.value))} />
                <span style={{ fontSize: 16, cursor: "pointer" }} onClick={() => setScale((s) => Math.min(ZOOM_MAX, s + ZOOM_STEP))}>
                  +
                </span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </>
  );
}

/* ================== SUB COMPONENTS ================== */
const Metric = ({ label, value, unit }) => (
  <div>
    <div style={{ fontSize: 11, fontWeight: 700, color: "var(--text-muted)", letterSpacing: 0.5, marginBottom: 4 }}>{label}</div>
    <div style={{ fontSize: 24, fontWeight: 400, fontFamily: "'JetBrains Mono', monospace", color: "var(--text-main)" }}>
      {value}
      {unit ? <span style={{ fontSize: 14, color: "var(--text-muted)", marginLeft: 4 }}>{unit}</span> : null}
    </div>
  </div>
);
