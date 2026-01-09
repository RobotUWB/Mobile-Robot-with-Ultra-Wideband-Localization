import React, { useEffect, useRef, useState } from "react";

/* ================== GLOBAL STYLES (INJECTED) ================== */
const GlobalStyles = () => (
  <style>{`
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&family=JetBrains+Mono:wght@400;700&display=swap');
    
    body {
      margin: 0;
      background-color: #0f172a; /* Slate 900 */
      font-family: 'Inter', sans-serif;
      -webkit-font-smoothing: antialiased;
    }

    /* Custom Range Slider */
    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      background: transparent;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      height: 16px;
      width: 16px;
      border-radius: 50%;
      background: #38bdf8;
      cursor: pointer;
      margin-top: -6px;
      box-shadow: 0 0 10px rgba(56, 189, 248, 0.5);
    }
    input[type=range]::-webkit-slider-runnable-track {
      width: 100%;
      height: 4px;
      cursor: pointer;
      background: #334155;
      border-radius: 2px;
    }
    
    /* Button Hover Effects */
    .btn-hover:hover { filter: brightness(1.2); transform: translateY(-1px); }
    .btn-active:active { transform: translateY(1px); }
  `}</style>
);

/* ================== CONFIG ================== */
const ESP32_IP = "192.168.88.16";
const SSE_URL = `http://${ESP32_IP}/pose`;
const CMD_URL = `http://${ESP32_IP}/cmd`;

const FIELD_W = 3000; // mm
const FIELD_H = 2000; // mm

const ZOOM_MIN = 0.05;
const ZOOM_MAX = 0.40; // เพิ่ม Max หน่อยเผื่อซูมเยอะ
const ZOOM_STEP = 0.01;
const ANCHORS = [
  { id: "A1", x_mm: 0, y_mm: 2000 },
  { id: "A2", x_mm: 3000, y_mm: 2000 },
  { id: "A3", x_mm: 0, y_mm: 0 },
  { id: "A4", x_mm: 3000, y_mm: 0 },
];

/* ================== UTILS ================== */
const clamp = (n, a, b) => Math.max(a, Math.min(b, n));
const mmToPx = (mm, scale) => mm * scale;

/* ================== APP ================== */
export default function App() {
  /* ---------- canvas & view ---------- */
  const canvasRef = useRef(null);
  const [scale, setScale] = useState(0.15); // เริ่มต้นใหญ่ขึ้นนิดนึง
  const scaleRef = useRef(scale);
  useEffect(() => { scaleRef.current = scale; }, [scale]);

  const [center] = useState({ x_mm: FIELD_W / 2, y_mm: FIELD_H / 2 });
  const centerRef = useRef(center);
  useEffect(() => { centerRef.current = center; }, [center]);

  const stopRobot = () => {
    sendCmd({ cmd: "STOP" });
  };

  /* ---------- robot pose ---------- */
  const poseRealRef = useRef(null);
  const [lastPose, setLastPose] = useState(null);
  const [uiPose, setUiPose] = useState({ x_mm: 0, y_mm: 0 });

  /* ---------- status ---------- */
  const [connected, setConnected] = useState(false);
  const [statusText, setStatusText] = useState("Waiting for connection...");
  
  /* ---------- UI toggle ---------- */
  const [showPoseLabel, setShowPoseLabel] = useState(true);
  const showPoseLabelRef = useRef(showPoseLabel);
  useEffect(() => { showPoseLabelRef.current = showPoseLabel; }, [showPoseLabel]);

  /* ================== SEND CMD ================== */
  const sendCmd = async (payload) => {
    try {
      await fetch(CMD_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
    } catch (e) {
      console.error("CMD error", e);
    }
  };

  /* ================== CANVAS CLICK ================== */
  const onCanvasClick = () => { };

  /* ================== SSE ================== */
  useEffect(() => {
    const es = new EventSource(SSE_URL);
    es.onopen = () => {
      setConnected(true);
      setStatusText("System Online");
    };
    es.onerror = () => {
      setConnected(false);
      setStatusText("Reconnecting...");
    };
    es.addEventListener("pose", (e) => {
      try {
        const j = JSON.parse(e.data);
        poseRealRef.current = { x_mm: j.x_mm, y_mm: j.y_mm };
        setUiPose({ x_mm: j.x_mm, y_mm: j.y_mm });
        setLastPose(j);
      } catch { }
    });
    return () => es.close();
  }, []);

  /* ================== DRAW LOOP ================== */
  useEffect(() => {
    const cvs = canvasRef.current;
    if (!cvs) return;
    const ctx = cvs.getContext("2d");
    let raf = 0;

    const draw = () => {
      const W = cvs.width;
      const H = cvs.height;
      const cx = W / 2;
      const cy = H / 2;
      const centerNow = centerRef.current;
      const scaleNow = scaleRef.current;

      const toPx = (x_mm, y_mm) => ({
        px: cx + mmToPx(x_mm - centerNow.x_mm, scaleNow),
        py: cy - mmToPx(y_mm - centerNow.y_mm, scaleNow),
      });

      /* ---- 1. Background Clear ---- */
      ctx.fillStyle = "#0f172a"; // Slate 900
      ctx.fillRect(0, 0, W, H);

      /* ---- 2. Grid Lines (ทุกๆ 500mm) ---- */
      ctx.strokeStyle = "rgba(56, 189, 248, 0.05)"; // Sky blue จางๆ
      ctx.lineWidth = 1;
      ctx.beginPath();
      
      const GRID_SIZE = 500;
      // Vertical Grids
      for (let x = 0; x <= FIELD_W; x += GRID_SIZE) {
         const top = toPx(x, FIELD_H);
         const bottom = toPx(x, 0);
         ctx.moveTo(top.px, top.py);
         ctx.lineTo(bottom.px, bottom.py);
      }
      // Horizontal Grids
      for (let y = 0; y <= FIELD_H; y += GRID_SIZE) {
         const left = toPx(0, y);
         const right = toPx(FIELD_W, y);
         ctx.moveTo(left.px, left.py);
         ctx.lineTo(right.px, right.py);
      }
      ctx.stroke();

      /* ---- 3. Field Boundary ---- */
      const p1 = toPx(0, 0);
      const p2 = toPx(FIELD_W, 0);
      const p3 = toPx(FIELD_W, FIELD_H);
      const p4 = toPx(0, FIELD_H);

      ctx.strokeStyle = "rgba(56, 189, 248, 0.4)"; // ขอบสนามสีฟ้าชัดขึ้น
      ctx.lineWidth = 2;
      ctx.setLineDash([]); // เส้นทึบ
      ctx.beginPath();
      ctx.moveTo(p1.px, p1.py);
      ctx.lineTo(p2.px, p2.py);
      ctx.lineTo(p3.px, p3.py);
      ctx.lineTo(p4.px, p4.py);
      ctx.closePath();
      ctx.stroke();

      /* ---- 4. Anchors ---- */
      ctx.font = "bold 12px Inter";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";

      for (const a of ANCHORS) {
        const { px, py } = toPx(a.x_mm, a.y_mm);
        
        // Glow effect
        ctx.shadowBlur = 10;
        ctx.shadowColor = "rgba(234, 179, 8, 0.5)"; // สีเหลืองทอง
        
        ctx.fillStyle = "#eab308";
        ctx.beginPath();
        ctx.arc(px, py, 4, 0, Math.PI * 2);
        ctx.fill();
        
        ctx.shadowBlur = 0; // Reset glow

        // Label Tag
        ctx.fillStyle = "rgba(255,255,255,0.6)";
        // ปรับตำแหน่ง Text หนีจุดนิดหน่อยตามมุม
        let tx = px, ty = py - 15;
        if(a.y_mm === 0) ty = py + 15; 
        
        ctx.fillText(a.id, tx, ty);
      }

      /* ---- 5. Robot ---- */
      if (poseRealRef.current) {
        const { x_mm, y_mm } = poseRealRef.current;
        const { px, py } = toPx(x_mm, y_mm);

        // Radar/Area circle
        ctx.fillStyle = "rgba(34, 197, 94, 0.15)";
        ctx.beginPath();
        ctx.arc(px, py, 20, 0, Math.PI * 2);
        ctx.fill();

        // Main Dot Glow
        ctx.shadowBlur = 15;
        ctx.shadowColor = "#22c55e"; // Green Neon

        // Main Dot
        ctx.fillStyle = "#4ade80";
        ctx.beginPath();
        ctx.arc(px, py, 6, 0, Math.PI * 2);
        ctx.fill();

        ctx.shadowBlur = 0; // Reset

        /* ---- Robot Label ---- */
        if (showPoseLabelRef.current) {
          const label = `X:${x_mm.toFixed(0)} Y:${y_mm.toFixed(0)}`;
          
          ctx.font = "11px JetBrains Mono";
          const metrics = ctx.measureText(label);
          const bgW = metrics.width + 12;
          const bgH = 22;
          
          ctx.fillStyle = "rgba(15, 23, 42, 0.8)";
          ctx.strokeStyle = "rgba(34, 197, 94, 0.5)";
          ctx.lineWidth = 1;
          
          const lx = px - bgW / 2;
          const ly = py - 35;

          ctx.beginPath();
          ctx.roundRect(lx, ly, bgW, bgH, 4);
          ctx.fill();
          ctx.stroke();

          ctx.fillStyle = "#4ade80";
          ctx.fillText(label, px, ly + bgH/2 + 1);
        }
      }

      raf = requestAnimationFrame(draw);
    };

    raf = requestAnimationFrame(draw);
    return () => cancelAnimationFrame(raf);
  }, []);

  /* ================== UI RENDER ================== */
  return (
    <>
      <GlobalStyles />
      <div style={{
        minHeight: "100vh",
        display: "flex",
        justifyContent: "center",
        alignItems: "center",
        padding: 20
      }}>
        
        {/* Main Dashboard Card */}
        <div style={{
          width: "100%",
          maxWidth: 1100,
          background: "rgba(30, 41, 59, 0.5)", // Semi-transparent card
          backdropFilter: "blur(12px)",
          borderRadius: 24,
          border: "1px solid rgba(255,255,255,0.08)",
          padding: 32,
          boxShadow: "0 25px 50px -12px rgba(0, 0, 0, 0.5)"
        }}>

          {/* ----- Header ----- */}
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 24 }}>
            <div>
              <h1 style={{ margin: 0, fontSize: 24, fontWeight: 700, color: "#f8fafc" }}>
                UWB Robot Localization
              </h1>
              <div style={{ display: "flex", alignItems: "center", gap: 8, marginTop: 6 }}>
                {/* Status Dot */}
                <div style={{
                  width: 8, height: 8, borderRadius: "50%",
                  background: connected ? "#22c55e" : "#ef4444",
                  boxShadow: connected ? "0 0 8px #22c55e" : "none"
                }} />
                <span style={{ fontSize: 13, color: connected ? "#86efac" : "#fca5a5" }}>
                  {statusText}
                </span>
              </div>
            </div>

            {/* Live Data Card */}
            <div style={{
              display: "flex", gap: 24,
              background: "#0f172a",
              padding: "12px 24px",
              borderRadius: 12,
              border: "1px solid rgba(255,255,255,0.05)"
            }}>
              <DataValue label="POSITION X" value={uiPose.x_mm.toFixed(0)} unit="mm" />
              <div style={{ width: 1, background: "#334155" }}></div>
              <DataValue label="POSITION Y" value={uiPose.y_mm.toFixed(0)} unit="mm" />
            </div>
          </div>

          {/* ----- Canvas Container ----- */}
          <div style={{
            position: "relative",
            borderRadius: 16,
            overflow: "hidden",
            border: "1px solid rgba(56, 189, 248, 0.2)",
            background: "#020617",
            boxShadow: "inset 0 0 40px rgba(0,0,0,0.5)"
          }}>
            <canvas
              ref={canvasRef}
              width={1036} // Fit container roughly
              height={600}
              onClick={onCanvasClick}
              style={{ width: "100%", display: "block", cursor: "crosshair" }}
            />
            
            {/* Overlay: Top Right Controls */}
            <div style={{ position: "absolute", top: 16, right: 16, display: "flex", flexDirection: "column", gap: 8 }}>
               <ToggleButton 
                 active={showPoseLabel} 
                 onClick={() => setShowPoseLabel(!showPoseLabel)}
                 label={showPoseLabel ? "Hide Tags" : "Show Tags"} 
               />
            </div>
          </div>

          {/* ----- Bottom Control Bar ----- */}
          <div style={{ 
            display: "flex", 
            justifyContent: "space-between", 
            alignItems: "center", 
            marginTop: 24,
            paddingTop: 24,
            borderTop: "1px solid rgba(255,255,255,0.05)"
          }}>
            
            {/* Zoom Controls */}
            <div style={{ display: "flex", alignItems: "center", gap: 16 }}>
              <span style={{ fontSize: 12, color: "#94a3b8", fontWeight: 600, letterSpacing: 1 }}>ZOOM LEVEL</span>
              <div style={{ display: "flex", alignItems: "center", gap: 12, background: "rgba(255,255,255,0.03)", padding: "8px 16px", borderRadius: 30 }}>
                <IconButton onClick={() => setScale(s => clamp(s - ZOOM_STEP, ZOOM_MIN, ZOOM_MAX))}>−</IconButton>
                <input
                  type="range"
                  min={ZOOM_MIN} max={ZOOM_MAX} step={ZOOM_STEP}
                  value={scale}
                  onChange={(e) => setScale(parseFloat(e.target.value))}
                  style={{ width: 100 }}
                />
                <IconButton onClick={() => setScale(s => clamp(s + ZOOM_STEP, ZOOM_MIN, ZOOM_MAX))}>+</IconButton>
              </div>
            </div>

            {/* Emergency Stop */}
            <button
              onClick={stopRobot}
              className="btn-hover btn-active"
              style={{
                background: "linear-gradient(135deg, #ef4444 0%, #b91c1c 100%)",
                color: "white",
                border: "none",
                borderRadius: 8,
                padding: "12px 32px",
                fontSize: 14,
                fontWeight: "700",
                letterSpacing: 1,
                cursor: "pointer",
                boxShadow: "0 4px 14px rgba(239, 68, 68, 0.4)",
                display: "flex", alignItems: "center", gap: 8
              }}
            >
              <span style={{ fontSize: 18 }}>⛔</span> EMERGENCY STOP
            </button>
          </div>

        </div>
      </div>
    </>
  );
}

/* ================== SUB COMPONENTS ================== */

const DataValue = ({ label, value, unit }) => (
  <div style={{ display: "flex", flexDirection: "column", minWidth: 80 }}>
    <span style={{ fontSize: 10, color: "#64748b", fontWeight: 700, letterSpacing: 0.5 }}>{label}</span>
    <span style={{ fontSize: 20, fontFamily: "'JetBrains Mono', monospace", color: "#f1f5f9", marginTop: 2 }}>
      {value} <span style={{ fontSize: 12, color: "#475569" }}>{unit}</span>
    </span>
  </div>
);

const IconButton = ({ children, onClick }) => (
  <button onClick={onClick} className="btn-hover" style={{
    width: 28, height: 28, borderRadius: "50%", border: "1px solid #475569",
    background: "transparent", color: "#cbd5e1", cursor: "pointer",
    display: "flex", alignItems: "center", justifyContent: "center", fontSize: 16
  }}>
    {children}
  </button>
);

const ToggleButton = ({ active, onClick, label }) => (
  <button onClick={onClick} className="btn-hover" style={{
    background: active ? "rgba(56, 189, 248, 0.2)" : "rgba(15, 23, 42, 0.6)",
    color: active ? "#38bdf8" : "#94a3b8",
    border: `1px solid ${active ? "#38bdf8" : "rgba(255,255,255,0.1)"}`,
    backdropFilter: "blur(4px)",
    borderRadius: 6,
    padding: "6px 12px",
    fontSize: 11, fontWeight: 600,
    cursor: "pointer", transition: "all 0.2s"
  }}>
    {label}
  </button>
);