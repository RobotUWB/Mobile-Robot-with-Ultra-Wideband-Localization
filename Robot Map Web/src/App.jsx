import React, { useEffect, useRef, useState } from "react";

/* ================== GLOBAL STYLES & ICONS ================== */
const Icons = {
  Play: () => <svg width="16" height="16" fill="currentColor" viewBox="0 0 24 24"><path d="M5 3l14 9-14 9V3z" /></svg>,
  Pause: () => <svg width="16" height="16" fill="currentColor" viewBox="0 0 24 24"><path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z" /></svg>,
  Refresh: () => <svg width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2" viewBox="0 0 24 24"><path d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" /></svg>,
  Wifi: () => <svg width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2" viewBox="0 0 24 24"><path d="M5 12.55a11 11 0 0114.08 0M1.64 9a15 15 0 0120.72 0M8.27 16a6 6 0 017.46 0M12 20h.01" /></svg>
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
const ESP32_IP = "192.168.88.69";
const API_URL = `http://${ESP32_IP}/json`; // ใช้ /json ตาม C++ code
const CMD_URL = `http://${ESP32_IP}/cmd`;
const FIELD_W = 3000;
const FIELD_H = 2000;
const ZOOM_MIN = 0.05, ZOOM_MAX = 0.40, ZOOM_STEP = 0.01;

/* ================== APP COMPONENT ================== */
export default function App() {
  /* --- State: Canvas & Data --- */
  const canvasRef = useRef(null);
  const [scale, setScale] = useState(0.15);
  // Default anchors
  const [anchors, setAnchors] = useState([
    { id: "A1", x_mm: 0, y_mm: 2000 }, { id: "A2", x_mm: 3000, y_mm: 2000 },
    { id: "A3", x_mm: 0, y_mm: 0 }, { id: "A4", x_mm: 3000, y_mm: 0 },
  ]);
  const [ranges, setRanges] = useState({ A1: 0, A2: 0, A3: 0, A4: 0 });
  const [pose, setPose] = useState({ x_mm: 0, y_mm: 0 });

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

  useEffect(() => { scaleRef.current = scale; }, [scale]);
  useEffect(() => { anchorsRef.current = anchors; }, [anchors]);
  useEffect(() => { showTagsRef.current = showTags; }, [showTags]);
  useEffect(() => { rangesRef.current = ranges; }, [ranges]);
  useEffect(() => { poseRef.current = pose; }, [pose]); // ✅ ทำให้จุดหุ่นอัปเดตตาม real-time

  /* --- Helper: Toast --- */
  const showToast = (msg, type = "info") => {
    setToast({ show: true, msg, type });
    setTimeout(() => setToast(prev => ({ ...prev, show: false })), 3000);
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
      setRefreshKey(k => k + 1);
      setIsRefreshing(false);
      showToast("Reconnecting...", "info");
    }, 800);
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
        // 🛠️ ตั้งค่าจูนทิศทางตรงนี้ (Calibration)
        // ==========================================
        const SWAP_XY = false; // ถ้าเดินหน้าแล้วจุดออกข้าง ให้แก้เป็น true
        const INVERT_X = false; // ถ้าเดินซ้ายแล้วจุดไปขวา ให้แก้เป็น true
        const INVERT_Y = false; // ถ้าเดินหน้าแล้วจุดถอยหลัง ให้แก้เป็น true

        // 1. Position Update
        const xVal = Number(j.x);
        const yVal = Number(j.y);

        if (Number.isFinite(xVal) && Number.isFinite(yVal)) {
          // ถ้า ESP ส่ง -1 แปลว่าหาตำแหน่งไม่ได้
          if (xVal !== -1 && yVal !== -1) {
            let rawX = xVal * 1000; // m -> mm
            let rawY = yVal * 1000;

            let finalX = SWAP_XY ? rawY : rawX;
            let finalY = SWAP_XY ? rawX : rawY;

            if (INVERT_X) finalX = FIELD_W - finalX;
            if (INVERT_Y) finalY = FIELD_H - finalY;

            setPose({ x_mm: finalX, y_mm: finalY });
          }
        }

        // 2. Ranges Update
        if (j.a && Array.isArray(j.a)) {
          setRanges({
            A1: parseFloat(j.a[0]) || 0,
            A2: parseFloat(j.a[1]) || 0,
            A3: parseFloat(j.a[2]) || 0,
            A4: parseFloat(j.a[3]) || 0
          });
        }

        // 3. Anchor Coordinates Update
        // ถ้าอยากกำหนดตำแหน่งเสาเองใน React ให้ลบส่วนนี้ทิ้ง
        // แต่ถ้าจะใช้ค่าจาก ESP32 ให้คงไว้แบบนี้
        if (j.anch_xy && Array.isArray(j.anch_xy)) {
          const newAnchors = j.anch_xy.map((p, i) => ({
            id: `A${i + 1}`,
            x_mm: p[0] * 1000,
            y_mm: p[1] * 1000
          }));

          // เช็คหน่อยว่าค่าที่ได้มาไม่เพี้ยน (กันไว้)
          if (newAnchors.length >= 3) {
            setAnchors(newAnchors);
          }
        }

      } catch (err) {
        setConnected(false);
      }
    };

    // ดึงข้อมูลทุก 100ms (10Hz) เพื่อความลื่นไหลแบบ Real-time
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

    // ใช้ตำแหน่งเสาเป็นกรอบอ้างอิง (เพื่อให้เส้นประตรงเสา)
    const getBoundsFromAnchors = (anchorsList) => {
      if (anchorsList && anchorsList.length) {
        let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;

        for (const a of anchorsList) {
          if (typeof a.x_mm !== "number" || typeof a.y_mm !== "number") continue;
          minX = Math.min(minX, a.x_mm);
          maxX = Math.max(maxX, a.x_mm);
          minY = Math.min(minY, a.y_mm);
          maxY = Math.max(maxY, a.y_mm);
        }

        // กันกรณีข้อมูลพัง/มีแค่จุดเดียว
        if (isFinite(minX) && isFinite(maxX) && isFinite(minY) && isFinite(maxY) && (minX !== maxX) && (minY !== maxY)) {
          return { minX, maxX, minY, maxY };
        }
      }

      // fallback ใช้ค่าคงที่เดิม
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


    const draw = () => {
      const W = cvs.width, H = cvs.height;
      const cx = W / 2, cy = H / 2;
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
        ctx.moveTo(p1.px, p1.py); ctx.lineTo(p2.px, p2.py);
      }

      const yStart = Math.floor(bounds.minY / STEP) * STEP;
      const yEnd = Math.ceil(bounds.maxY / STEP) * STEP;
      for (let y = yStart; y <= yEnd; y += STEP) {
        let p1 = toPx(bounds.minX, y, cx, cy, s, bounds);
        let p2 = toPx(bounds.maxX, y, cx, cy, s, bounds);
        ctx.moveTo(p1.px, p1.py); ctx.lineTo(p2.px, p2.py);
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
      ctx.moveTo(b1.px, b1.py); ctx.lineTo(b2.px, b2.py);
      ctx.lineTo(b3.px, b3.py); ctx.lineTo(b4.px, b4.py);
      ctx.closePath();
      ctx.stroke();
      ctx.setLineDash([]);

      // Anchors
      for (const a of anchorsRef.current) {
        const { px, py } = toPx(a.x_mm, a.y_mm, cx, cy, s, bounds);

        // ดึงค่าระยะทางออกมาเช็ค
        const rangeVal = currentRanges[a.id] || 0;
        // เช็คสถานะ: ถ้าระยะมากกว่า 0 ถือว่า Online (สีเขียว), ถ้า -1 หรือ 0 คือ Offline (สีเทา)
        const isOnline = rangeVal > 0;

        // กำหนดสี
        // สีเขียว: #22c55e (Success), สีเทา: #475569 (Muted)
        const colorMain = isOnline ? "#22c55e" : "#475569";
        const colorGlow = isOnline ? "rgba(34, 197, 94, 0.8)" : "transparent";
        const colorText = isOnline ? "#ffffff" : "#64748b"; // ตัวหนังสือขาว หรือ เทาจางๆ

        // 1. วาดจุดตรงกลาง (Main Dot)
        ctx.shadowBlur = isOnline ? 15 : 0; // มีแสงเฉพาะตอน Online
        ctx.shadowColor = colorGlow;
        ctx.fillStyle = colorMain;
        ctx.beginPath();
        ctx.arc(px, py, isOnline ? 6 : 4, 0, Math.PI * 2); // ถ้า Offline จุดจะเล็กกว่านิดนึง
        ctx.fill();
        ctx.shadowBlur = 0; // Reset shadow

        // 2. วาดวงแหวนรอบๆ และ Text
        if (show) {
          // วงแหวน
          ctx.fillStyle = isOnline ? "rgba(34, 197, 94, 0.1)" : "transparent";
          ctx.strokeStyle = colorMain;
          ctx.lineWidth = 1;

          ctx.beginPath();
          // ถ้า Offline ให้เส้นวงแหวนเป็นเส้นประ ดูเก๋ๆ ว่าสัญญาณขาด
          if (!isOnline) ctx.setLineDash([4, 4]);
          ctx.arc(px, py, 14, 0, Math.PI * 2);
          ctx.stroke();
          ctx.fill();
          ctx.setLineDash([]); // Reset LineDash

          // Text บอกชื่อและระยะ
          ctx.fillStyle = colorText;
          ctx.font = "bold 14px Inter";
          ctx.textAlign = "center";

          const labelText = `${a.id} ${rangeVal.toFixed(2)}m`;
          ctx.fillText(labelText, px, py - 20);
        }
      }
      // Robot
      const { x_mm, y_mm } = poseRef.current;
      const { px, py } = toPx(x_mm, y_mm, cx, cy, s, bounds);

      ctx.shadowBlur = 20; ctx.shadowColor = "rgba(249, 115, 22, 0.65)"; // orange glow
      ctx.fillStyle = "#f97316"; // orange
      ctx.beginPath(); ctx.arc(px, py, 8, 0, Math.PI * 2); ctx.fill();

      ctx.strokeStyle = "rgba(249, 115, 22, 0.45)"; // orange ring
      ctx.lineWidth = 2;
      ctx.beginPath(); ctx.arc(px, py, 18, 0, Math.PI * 2); ctx.stroke();
      ctx.shadowBlur = 0;


      if (show) {
        const label = `X:${(x_mm / 1000).toFixed(2)} Y:${(y_mm / 1000).toFixed(2)}`;
        const tw = ctx.measureText(label).width + 16;
        ctx.fillStyle = "rgba(15, 23, 42, 0.8)";
        ctx.strokeStyle = "rgba(255,255,255,0.2)";
        ctx.beginPath(); ctx.roundRect(px - tw / 2, py + 20, tw, 24, 6); ctx.fill(); ctx.stroke();

        ctx.fillStyle = "#fff"; ctx.font = "11px JetBrains Mono";
        ctx.fillText(label, px, py + 36);
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
      <div style={{
        position: "fixed", top: 24, right: 24, zIndex: 99,
        background: toast.type === "error" ? "rgba(239, 68, 68, 0.9)" : toast.type === "success" ? "rgba(34, 197, 94, 0.9)" : "rgba(59, 130, 246, 0.9)",
        color: "#fff", padding: "12px 24px", borderRadius: 8,
        transform: toast.show ? "translateY(0)" : "translateY(-100px)",
        opacity: toast.show ? 1 : 0, transition: "all 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275)",
        fontWeight: 600, fontSize: 14, boxShadow: "0 10px 15px -3px rgba(0,0,0,0.3)"
      }}>
        {toast.msg}
      </div>

      <div style={{ minHeight: "100vh", display: "flex", flexDirection: "column", maxWidth: 1400, margin: "0 auto", padding: 20, gap: 20 }}>

        {/* --- Header / Status Deck --- */}
        <header className="glass-panel" style={{ borderRadius: 16, padding: "20px 24px", display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          <div style={{ display: "flex", gap: 16, alignItems: "center" }}>
            <div style={{ width: 48, height: 48, background: "rgba(56, 189, 248, 0.1)", borderRadius: 12, display: "flex", alignItems: "center", justifyContent: "center", color: "var(--accent)" }}>
              <Icons.Wifi />
            </div>
            <div>
              <h1 style={{ margin: 0, fontSize: 20, fontWeight: 700 }}>UWB Localization</h1>
              <div style={{ display: "flex", alignItems: "center", gap: 8, marginTop: 4 }}>
                <div className="pulse-dot" style={{ width: 8, height: 8, borderRadius: "50%", background: connected ? "var(--success)" : "var(--danger)" }} />
                <span style={{ fontSize: 15, color: connected ? "var(--success)" : "var(--danger)", fontWeight: 600 }}>
                  {connected ? "SYSTEM ONLINE" : "DISCONNECTED"}
                </span>
              </div>
            </div>
          </div>

          {/* Big Metrics */}
          <div style={{ display: "flex", gap: 32 }}>
            <Metric label="COORD X" value={(pose.x_mm / 1000).toFixed(2)} unit="m" />
            <div style={{ width: 1, background: "var(--border)" }} />
            <Metric label="COORD Y" value={(pose.y_mm / 1000).toFixed(2)} unit="m" />
          </div>
        </header>

        {/* --- Main Grid --- */}
        <div style={{ display: "grid", gridTemplateColumns: "300px 1fr", gap: 20, flex: 1 }}>

          {/* --- Left Sidebar (Controls) --- */}
          <div style={{ display: "flex", flexDirection: "column", gap: 20 }}>

            {/* 1. Anchors Status */}
            <div className="glass-panel" style={{ borderRadius: 16, padding: 20 }}>
              <div style={{ fontSize: 15, fontWeight: 700, color: "var(--text-muted)", marginBottom: 16, letterSpacing: 1 }}>ANCHOR DISTANCES</div>
              <div style={{ display: "grid", gap: 12 }}>
                {Object.entries(ranges).map(([k, v]) => (
                  <div key={k} style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                    <span style={{ color: "#cbd5e1", fontSize: "16px", fontWeight: "700" }}>{k}</span>
                    <span className="mono-font" style={{ fontSize: "14px", color: "var(--accent)" }}>{(v).toFixed(2)}m</span>
                  </div>
                ))}
              </div>
            </div>

            {/* 2. Main Controls */}
            <div style={{ marginTop: "auto" }}>
              <button onClick={() => {
                if (isPaused) { sendCmd({ cmd: "RESUME" }); setIsPaused(false); }
                else { sendCmd({ cmd: "PAUSE" }); setIsPaused(true); }
              }}
                className="btn-base"
                style={{
                  width: "100%", padding: 16, borderRadius: 12, marginBottom: 12,
                  background: isPaused ? "var(--success)" : "#334155", color: "#fff",
                  border: "none", fontSize: 14
                }}>
                {isPaused ? <><Icons.Play /> RESUME</> : <><Icons.Pause /> PAUSE ROBOT</>}
              </button>

              <button onClick={() => { sendCmd({ cmd: "STOP" }); setIsPaused(false); showToast("EMERGENCY STOP TRIGGERED", "error"); }}
                className="btn-base"
                style={{
                  width: "100%", padding: 18, borderRadius: 12,
                  background: "linear-gradient(135deg, #ef4444 0%, #dc2626 100%)",
                  color: "#fff", border: "none", fontSize: 16, boxShadow: "0 4px 14px rgba(220, 38, 38, 0.4)"
                }}>
                ⛔ EMERGENCY STOP
              </button>
            </div>
          </div>

          {/* --- Right: Visualization --- */}
          <div className="glass-panel" style={{ borderRadius: 16, padding: 0, position: "relative", overflow: "hidden", display: "flex", flexDirection: "column" }}>

            {/* Map Toolbar */}
            <div style={{ position: "absolute", top: 16, right: 16, display: "flex", gap: 8, zIndex: 10 }}>
              {/* ปุ่ม Refresh ถูกลบออกไปแล้ว */}
              <button onClick={() => setShowTags(!showTags)} className="btn-base glass-panel" style={{ padding: "0 12px", height: 36, borderRadius: 8, fontSize: 12, color: showTags ? "var(--accent)" : "var(--text-muted)" }}>
                {showTags ? "HIDE TAGS" : "SHOW TAGS"}
              </button>
            </div>

            {/* Canvas */}
            <div style={{ flex: 1, background: "#020617", position: "relative" }}>
              <canvas ref={canvasRef} width={1200} height={700} style={{ width: "100%", height: "100%", objectFit: "contain", display: "block" }} />
            </div>

            {/* Zoom Footer */}
            <div style={{ padding: "12px 24px", borderTop: "1px solid var(--border)", display: "flex", justifyContent: "space-between", alignItems: "center", background: "rgba(15, 23, 42, 0.4)" }}>
              <span style={{ fontSize: 15, color: "var(--text-muted)" }}>MAP VIEW: {(scale * 100).toFixed(0)}%</span>
              <div style={{ display: "flex", alignItems: "center", gap: 12, width: 200 }}>
                <span style={{ fontSize: 16, cursor: "pointer" }} onClick={() => setScale(s => Math.max(ZOOM_MIN, s - ZOOM_STEP))}>-</span>
                <input type="range" min={ZOOM_MIN} max={ZOOM_MAX} step={ZOOM_STEP} value={scale} onChange={e => setScale(parseFloat(e.target.value))} />
                <span style={{ fontSize: 16, cursor: "pointer" }} onClick={() => setScale(s => Math.min(ZOOM_MAX, s + ZOOM_STEP))}>+</span>
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
      {value}<span style={{ fontSize: 14, color: "var(--text-muted)", marginLeft: 4 }}>{unit}</span>
    </div>
  </div>
);